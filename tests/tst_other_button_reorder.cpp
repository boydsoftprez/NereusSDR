// =================================================================
// tests/tst_other_button_reorder.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test. It checks the other-buttons
// reorder drag against Thetis's behaviour (MeterManager.cs:9322-9390,
// :9546-9550 [v2.10.3.15]) and the editor's button list; no upstream
// logic is ported here.
//
// Container item parity Task 59:
//   - An Alt press inside a container reaches the item (the arranging
//     hold filter lets it through), and Shift during the drag starts no
//     arranging hold.
//   - Alt-press, then release over another button: it moves there and the
//     buttons between shift by one; no click. With Shift: the two swap.
//   - Released on itself or off the buttons: nothing moves.
//   - While Fade on RX / TX covers the grid, the press does nothing.
//   - The order is saved (field 28) and loads the same; Reset layout in
//     the editor restores the first order.
//   - A copy of the grid (a preview) never reorders another.
//   - The editor lists every offered button under its Thetis heading,
//     "Tuning" for "VFOs", with no VFO A / B or RX1 / RX2 in any row.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26  J.J. Boyd / KG4VCF  Container item parity Task 59.
//                                    AI-assisted via Anthropic Claude
//                                    Code.
// =================================================================

#include <QtTest/QtTest>
#include <QCheckBox>
#include <QDir>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QSplitter>
#include <QTemporaryDir>

#include "OperatorWording.h"

#include "core/AppSettings.h"
#include "gui/containers/ArrangeHoldFilter.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/meter_property_editors/BaseItemEditor.h"
#include "gui/containers/meter_property_editors/OtherButtonItemEditor.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/OtherButtonCatalog.h"
#include "gui/meters/OtherButtonItem.h"

#include <memory>

using namespace NereusSDR;
using Id = OtherButtonItem::ButtonId;

namespace {

constexpr int kSize = 600;
constexpr int kHoldDelayMs = 20;

// The fifteen buttons a new grid draws, in grid order.
const QList<Id> kShown = {
    Id::Mon, Id::Tun, Id::Mox, Id::TwoTon, Id::Dup, Id::PsA,
    Id::Anf, Id::Snb, Id::Mnf, Id::PeakHold, Id::Ctun,
    Id::Vac1, Id::Vac2, Id::Mute, Id::Bin,
};

// The centre third of the button drawn `ordinal`-th on a 600 x 600 widget
// (six columns), as tst_other_button_item's clickShownButton places it.
QPointF pointOfShown(const ButtonBoxItem& item, int ordinal)
{
    const double cell = kSize * 0.96 / item.columns();
    return {12.0 + (ordinal % item.columns()) * cell + cell / 3.0,
            12.0 + (ordinal / item.columns()) * cell + cell / 3.0};
}

QPointF pointOf(const OtherButtonItem& item, Id id)
{
    const int slot = item.indexOf(id);
    int ordinal = 0;
    for (int i = 0; i < slot; ++i) {
        if (item.ButtonBoxItem::isButtonShown(i)) { ++ordinal; }
    }
    return pointOfShown(item, ordinal);
}

// The ids drawn, in grid order.
QList<Id> shownOrder(const OtherButtonItem& item)
{
    QList<Id> order;
    const QVector<short> map = item.buttonMap();
    for (int slot = 0; slot < map.size(); ++slot) {
        if (item.ButtonBoxItem::isButtonShown(slot)) { order.append(Id(map[slot])); }
    }
    return order;
}

QVector<short> identityMap()
{
    QVector<short> map(OtherButtonItem::kSlotCount);
    for (int i = 0; i < map.size(); ++i) { map[i] = short(i); }
    return map;
}

// Press with `pressMods` at `from`, move to `to`, release there with
// `releaseMods`, straight to the item.
void drag(OtherButtonItem& item, QPointF from, QPointF to, Qt::KeyboardModifiers pressMods,
          Qt::KeyboardModifiers releaseMods)
{
    QMouseEvent press(QEvent::MouseButtonPress, from, from, Qt::LeftButton, Qt::LeftButton, pressMods);
    QMouseEvent move(QEvent::MouseMove, to, to, Qt::NoButton, Qt::LeftButton, pressMods);
    QMouseEvent release(QEvent::MouseButtonRelease, to, to, Qt::LeftButton, Qt::NoButton, releaseMods);
    item.handleMousePress(&press, kSize, kSize);
    item.handleMouseMove(&move, kSize, kSize);
    item.handleMouseRelease(&release, kSize, kSize);
}

// Ruling D2: no row says VFO A / B (VFOa, VFOb) or RX1 / RX2.
const QRegularExpression& retiredWords()
{
    static const QRegularExpression words(QStringLiteral("\\bVFO\\s*[AB]\\b|\\bRX\\s*[12]\\b"),
                                          QRegularExpression::CaseInsensitiveOption);
    return words;
}

QImage render(OtherButtonItem& item)
{
    QImage image(kSize, kSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(QColor(0x0f, 0x0f, 0x1a));
    QPainter p(&image);
    item.paint(p, kSize, kSize);
    return image;
}

} // namespace

class TstOtherButtonReorder : public QObject {
    Q_OBJECT

private slots:
    // ── Input path ─────────────────────────────────────────────────────

    // An Alt press on a grid inside a container reaches the item through
    // the app's arranging hold filter; Shift pressed mid-drag starts no
    // arranging hold, and the release with Shift swaps.
    void altPressInsideAContainerReachesTheItem()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings{directory.filePath(QStringLiteral("reorder.xml"))};
        ContainerWorkspaceStore store{settings};
        QWidget dock;
        QSplitter splitter;
        ContainerContentRegistry registry;
        ContainerManager manager{&dock, &splitter};
        manager.setWorkspaceAdapter(&store, &registry);
        ArrangeHoldFilter filter(&manager, nullptr);
        filter.setHoldDelayForTest(kHoldDelayMs);

        dock.resize(kSize + 40, kSize + 40);
        auto* meter = new MeterWidget(&dock);
        meter->setGeometry(0, 0, kSize, kSize);
        auto* item = new OtherButtonItem;
        item->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        meter->addItem(item);

        QSignalSpy clicked(item, &OtherButtonItem::otherButtonClicked);
        const QPointF from = pointOf(*item, Id::Mon);
        const QPointF to = pointOf(*item, Id::Mox);
        const Qt::KeyboardModifiers alt = Qt::AltModifier;
        QMouseEvent press(QEvent::MouseButtonPress, from, meter->mapToGlobal(from), Qt::LeftButton,
                          Qt::LeftButton, alt);
        QApplication::sendEvent(meter, &press);
        QVERIFY(press.isAccepted());

        // Shift while the button is down (and Alt held): no arranging hold.
        QKeyEvent shift(QEvent::KeyPress, Qt::Key_Shift, alt | Qt::ShiftModifier);
        QApplication::sendEvent(meter, &shift);
        QTest::qWait(kHoldDelayMs * 4);
        QVERIFY(!manager.arrangeHeld());

        QMouseEvent move(QEvent::MouseMove, to, meter->mapToGlobal(to), Qt::NoButton, Qt::LeftButton,
                         alt | Qt::ShiftModifier);
        QApplication::sendEvent(meter, &move);
        QMouseEvent release(QEvent::MouseButtonRelease, to, meter->mapToGlobal(to), Qt::LeftButton,
                            Qt::NoButton, alt | Qt::ShiftModifier);
        QApplication::sendEvent(meter, &release);
        QKeyEvent shiftUp(QEvent::KeyRelease, Qt::Key_Shift, alt);
        QApplication::sendEvent(meter, &shiftUp);

        QCOMPARE(clicked.count(), 0);
        QVERIFY(!manager.arrangeHeld());
        QList<Id> expected = kShown;
        expected.swapItemsAt(0, 2);
        QCOMPARE(shownOrder(*item), expected);
    }

    // ── The drag ───────────────────────────────────────────────────────

    void altDragMovesTheButtonAndClicksNothing()
    {
        OtherButtonItem item;
        QSignalSpy clicked(&item, &OtherButtonItem::otherButtonClicked);
        QSignalSpy macros(&item, &OtherButtonItem::macroTriggered);

        // Forward: MON onto MOX; TUN and MOX shift back one.
        drag(item, pointOf(item, Id::Mon), pointOf(item, Id::Mox), Qt::AltModifier, Qt::AltModifier);
        QCOMPARE(clicked.count(), 0);
        QList<Id> expected = kShown;
        expected.move(0, 2);
        QCOMPARE(shownOrder(item), expected);
        QVector<short> map = identityMap();
        map[2] = 3; map[3] = 4; map[4] = 2;
        QCOMPARE(item.buttonMap(), map);

        // Backward: BIN onto TUN; every slot from TUN's on shifts up one,
        // the hidden ones between included (the map covers all 384).
        const int binSlot = item.indexOf(Id::Bin);
        const int tunSlot = item.indexOf(Id::Tun);
        const QVector<short> before = item.buttonMap();
        drag(item, pointOf(item, Id::Bin), pointOf(item, Id::Tun), Qt::AltModifier, Qt::NoModifier);
        QCOMPARE(clicked.count(), 0);
        QCOMPARE(macros.count(), 0);
        expected.move(expected.indexOf(Id::Bin), expected.indexOf(Id::Tun));
        QCOMPARE(shownOrder(item), expected);
        const QVector<short> after = item.buttonMap();
        QCOMPARE(after[tunSlot], short(Id::Bin));
        for (int slot = tunSlot + 1; slot <= binSlot; ++slot) { QCOMPARE(after[slot], before[slot - 1]); }
        for (int slot = binSlot + 1; slot < after.size(); ++slot) { QCOMPARE(after[slot], before[slot]); }
        QCOMPARE(item.buttonMap().size(), OtherButtonItem::kSlotCount);

        // A button's state goes with it.
        item.setButtonState(Id::Mon, true);
        drag(item, pointOf(item, Id::Mon), pointOf(item, Id::Mute), Qt::AltModifier, Qt::NoModifier);
        QVERIFY(item.buttonState(Id::Mon));
        QVERIFY(!item.buttonState(Id::Mute));
        QCOMPARE(clicked.count(), 0);

        // Without Alt it is a click, and nothing moves.
        const QVector<short> settled = item.buttonMap();
        drag(item, pointOf(item, Id::Mnf), pointOf(item, Id::Mnf), Qt::NoModifier, Qt::NoModifier);
        QCOMPARE(clicked.count(), 1);
        QCOMPARE(clicked.first().first().toInt(), int(Id::Mnf));
        QCOMPARE(item.buttonMap(), settled);
    }

    void shiftAtTheReleaseSwapsTheTwo()
    {
        OtherButtonItem item;
        QSignalSpy clicked(&item, &OtherButtonItem::otherButtonClicked);
        drag(item, pointOf(item, Id::Mon), pointOf(item, Id::Ctun), Qt::AltModifier,
             Qt::AltModifier | Qt::ShiftModifier);
        QCOMPARE(clicked.count(), 0);
        QList<Id> expected = kShown;
        expected.swapItemsAt(expected.indexOf(Id::Mon), expected.indexOf(Id::Ctun));
        QCOMPARE(shownOrder(item), expected);
        QVector<short> map = identityMap();
        map[int(Id::Mon)] = short(Id::Ctun);
        map[int(Id::Ctun)] = short(Id::Mon);
        QCOMPARE(item.buttonMap(), map);
    }

    void aDropOnItselfOrOffTheButtonsMovesNothing()
    {
        OtherButtonItem item;
        QSignalSpy clicked(&item, &OtherButtonItem::otherButtonClicked);
        drag(item, pointOf(item, Id::Mox), pointOf(item, Id::Mox), Qt::AltModifier, Qt::NoModifier);
        // Below the last row of buttons: no button there.
        drag(item, pointOf(item, Id::Mox), QPointF(kSize / 2.0, kSize - 4.0), Qt::AltModifier,
             Qt::NoModifier);
        QCOMPARE(item.buttonMap(), identityMap());
        QCOMPARE(clicked.count(), 0);
    }

    // MeterManager.cs:9324-9325: while the fade covers the grid a press
    // does nothing, an Alt-drag included.
    void aPressOnAFadedGridDoesNothing()
    {
        for (const bool transmitting : {false, true}) {
            OtherButtonItem item;
            if (transmitting) { item.setFadeOnTx(true); } else { item.setFadeOnRx(true); }
            item.setTransmitting(transmitting);
            QSignalSpy clicked(&item, &OtherButtonItem::otherButtonClicked);

            const QPointF from = pointOf(item, Id::Mon);
            QMouseEvent press(QEvent::MouseButtonPress, from, from, Qt::LeftButton, Qt::LeftButton,
                              Qt::AltModifier);
            QVERIFY(!item.handleMousePress(&press, kSize, kSize));
            drag(item, from, pointOf(item, Id::Mox), Qt::AltModifier, Qt::NoModifier);
            drag(item, from, pointOf(item, Id::Mox), Qt::AltModifier, Qt::ShiftModifier);
            drag(item, from, from, Qt::NoModifier, Qt::NoModifier);
            QCOMPARE(item.buttonMap(), identityMap());
            QCOMPARE(clicked.count(), 0);

            // The fade coming on mid-drag: the release moves nothing.
            item.setTransmitting(!transmitting);
            QMouseEvent unfaded(QEvent::MouseButtonPress, from, from, Qt::LeftButton,
                                Qt::LeftButton, Qt::AltModifier);
            QVERIFY(item.handleMousePress(&unfaded, kSize, kSize));
            item.setTransmitting(transmitting);
            const QPointF to = pointOf(item, Id::Mox);
            QMouseEvent release(QEvent::MouseButtonRelease, to, to, Qt::LeftButton, Qt::NoButton,
                                Qt::NoModifier);
            item.handleMouseRelease(&release, kSize, kSize);
            QCOMPARE(item.buttonMap(), identityMap());
            QCOMPARE(clicked.count(), 0);
        }
    }

    // The dragged button follows the pointer, outlined; the grid itself is
    // drawn as before.
    void theDraggedButtonFollowsThePointer()
    {
        OtherButtonItem item;
        const QImage still = render(item);
        const QPointF from = pointOf(item, Id::Mon);
        const QPointF to = pointOf(item, Id::Mute);
        // Below the three rows of buttons, so only the drag face is drawn there.
        const QPointF between(kSize / 2.0, kSize * 0.75);
        QMouseEvent press(QEvent::MouseButtonPress, from, from, Qt::LeftButton, Qt::LeftButton,
                          Qt::AltModifier);
        QVERIFY(item.handleMousePress(&press, kSize, kSize));
        QCOMPARE(render(item), still);  // no face until the pointer moves
        QMouseEvent move(QEvent::MouseMove, to, to, Qt::NoButton, Qt::LeftButton, Qt::AltModifier);
        QVERIFY(item.handleMouseMove(&move, kSize, kSize));
        const QImage dragging = render(item);
        QVERIFY(dragging != still);
        QMouseEvent moveBetween(QEvent::MouseMove, between, between, Qt::NoButton, Qt::LeftButton,
                                Qt::AltModifier);
        QVERIFY(item.handleMouseMove(&moveBetween, kSize, kSize));
        const QImage offTarget = render(item);
        QVERIFY(offTarget.pixelColor(between.toPoint()) != still.pixelColor(between.toPoint()));

        const QString dir = qEnvironmentVariable("NEREUS_OTHER_BUTTONS_CAPTURE_DIR");
        if (!dir.isEmpty()) {
            QDir().mkpath(dir);
            QVERIFY(dragging.save(QDir(dir).filePath(QStringLiteral("other-buttons-mid-drag.png"))));
            QVERIFY(offTarget.save(QDir(dir).filePath(QStringLiteral("other-buttons-off-target.png"))));
        }

        // Let go off the grid: the face goes and nothing moved.
        QMouseEvent lost(QEvent::MouseMove, between, between, Qt::NoButton, Qt::NoButton,
                         Qt::NoModifier);
        QVERIFY(item.handleMouseMove(&lost, kSize, kSize));
        QCOMPARE(item.buttonMap(), identityMap());
        QCOMPARE(render(item), still);
    }

    // ── Saved order ────────────────────────────────────────────────────

    void theOrderIsSavedAndLoadsTheSame()
    {
        OtherButtonItem item;
        drag(item, pointOf(item, Id::Mon), pointOf(item, Id::Bin), Qt::AltModifier, Qt::NoModifier);
        drag(item, pointOf(item, Id::Snb), pointOf(item, Id::Tun), Qt::AltModifier, Qt::ShiftModifier);
        const QString record = item.serialize();
        const QStringList fields = record.split(QLatin1Char('|'));
        QCOMPARE(fields.size(), 29);
        QVERIFY(!fields[28].isEmpty());
        QCOMPARE(fields[28].split(QLatin1Char(',')).size(), OtherButtonItem::kSlotCount);

        OtherButtonItem loaded;
        QVERIFY(loaded.deserialize(record));
        QCOMPARE(loaded.buttonMap(), item.buttonMap());
        QCOMPARE(shownOrder(loaded), shownOrder(item));
        QCOMPARE(loaded.serialize(), record);

        // Through the registry: capture, then build the entry again.
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("OTHERBTNS"));
        ContentEntry saved = entry;
        saved.config.insert(QStringLiteral("legacyRecord"), record);
        std::unique_ptr<MeterItem> rebuilt(registry.createMeterItem(saved, nullptr, ContentRenderMode::Live));
        auto* other = qobject_cast<OtherButtonItem*>(rebuilt.get());
        QVERIFY(other);
        QCOMPARE(other->buttonMap(), item.buttonMap());

        // Reset: the first order, and field 28 empty again.
        loaded.resetButtonMap();
        QCOMPARE(loaded.buttonMap(), identityMap());
        QVERIFY(loaded.serialize().split(QLatin1Char('|'))[28].isEmpty());
    }

    // A copy (the settings preview) and the grid it was made from reorder
    // independently.
    void aCopyNeverReordersTheGridItCameFrom()
    {
        ContainerContentRegistry registry;
        const ContentEntry entry = registry.makeEntry(QStringLiteral("OTHERBTNS"));
        std::unique_ptr<MeterItem> live(registry.createMeterItem(entry, nullptr, ContentRenderMode::Live));
        std::unique_ptr<MeterItem> preview(registry.createMeterItem(entry, nullptr, ContentRenderMode::Preview));
        auto* liveGrid = qobject_cast<OtherButtonItem*>(live.get());
        auto* previewGrid = qobject_cast<OtherButtonItem*>(preview.get());
        QVERIFY(liveGrid && previewGrid);
        liveGrid->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        previewGrid->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        const QVector<short> liveBefore = liveGrid->buttonMap();
        drag(*previewGrid, pointOf(*previewGrid, Id::Mon), pointOf(*previewGrid, Id::Mox),
             Qt::AltModifier, Qt::NoModifier);
        QVERIFY(previewGrid->buttonMap() != liveBefore);
        QCOMPARE(liveGrid->buttonMap(), liveBefore);
    }

    // ── The editor ─────────────────────────────────────────────────────

    void theEditorListsEveryOfferedButton()
    {
        OtherButtonItem item;
        OtherButtonItemEditor editor;
        QSignalSpy changed(&editor, &BaseItemEditor::propertyChanged);
        editor.setItem(&item);

        int offered = 0;
        for (int value = 0; value < OtherButtonItem::kSlotCount; ++value) {
            const Id id = Id(value);
            auto* check = editor.findChild<QCheckBox*>(QStringLiteral("otherVisible_%1").arg(value));
            const bool listed = check != nullptr;
            QCOMPARE(listed, OtherButtonCatalog::isDefined(id) && item.isOffered(id));
            if (!check) { continue; }
            ++offered;
            QCOMPARE(check->isChecked(), item.isButtonVisible(id));
            QVERIFY(!check->toolTip().isEmpty());
        }
        // The 33 buttons NereusSDR has had (Power excepted) and 31 macros.
        QCOMPARE(offered, 33 + OtherButtonItem::kMacroCount);

        QStringList texts;
        for (const QLabel* label : editor.findChildren<QLabel*>()) {
            texts << label->text() << label->toolTip();
        }
        for (const QCheckBox* check : editor.findChildren<QCheckBox*>()) { texts << check->toolTip(); }
        for (const QString& text : std::as_const(texts)) {
            QVERIFY2(!retiredWords().match(text).hasMatch(), qPrintable(text));
            QVERIFY2(!text.contains(QLatin1String("VFOa")), qPrintable(text));
        }
        // The Thetis headings of the offered buttons, in Thetis order.
        QStringList headings;
        for (const QString& section : {QStringLiteral("General"), QStringLiteral("Noise / ATT"),
                                       QStringLiteral("Display"), QStringLiteral("Audio / DSP"),
                                       QStringLiteral("Macros")}) {
            QVERIFY2(texts.contains(section), qPrintable(section));
            headings << section;
        }
        QVERIFY(!texts.contains(QStringLiteral("VFOs")));
        // RX2's row is a slice; VAC is VAX.
        QVERIFY(texts.contains(QStringLiteral("Slice B")));
        QVERIFY(texts.contains(QStringLiteral("VAX 1 on/off")));

        // Toggling a row shows or hides that button.
        auto* mon = editor.findChild<QCheckBox*>(QStringLiteral("otherVisible_%1").arg(int(Id::Mon)));
        QVERIFY(mon && mon->isChecked());
        mon->setChecked(false);
        QVERIFY(!item.isButtonVisible(Id::Mon));
        QVERIFY(!item.isButtonShown(Id::Mon));
        mon->setChecked(true);
        QVERIFY(item.isButtonShown(Id::Mon));
        QCOMPARE(changed.count(), 2);

        // Reset layout puts the first order back.
        item.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        drag(item, pointOf(item, Id::Mon), pointOf(item, Id::Bin), Qt::AltModifier, Qt::NoModifier);
        QVERIFY(item.buttonMap() != identityMap());
        auto* reset = editor.findChild<QPushButton*>(QStringLiteral("otherResetLayout"));
        QVERIFY(reset);
        QVERIFY(OperatorWording::isPlain(reset->text()));
        QVERIFY(OperatorWording::isPlain(reset->toolTip()));
        reset->click();
        QCOMPARE(item.buttonMap(), identityMap());
        QCOMPARE(changed.count(), 3);

        const QString dir = qEnvironmentVariable("NEREUS_OTHER_BUTTONS_CAPTURE_DIR");
        if (!dir.isEmpty()) {
            editor.resize(420, editor.sizeHint().height());
            QDir().mkpath(dir);
            QVERIFY(editor.grab().save(QDir(dir).filePath(QStringLiteral("other-buttons-editor.png"))));
        }
    }

    // Every heading and row the Thetis table has, as the editor would show
    // it once each button is offered (Tasks 60-62 offer more).
    void everyThetisRowReadsInNereusWords()
    {
        const auto rows = OtherButtonItemEditor::visibilityRows([](Id) { return true; });
        int defined = 0;
        for (int value = 0; value < OtherButtonItem::kSlotCount; ++value) {
            if (OtherButtonCatalog::isDefined(Id(value))) { ++defined; }
        }
        QCOMPARE(rows.size(), defined);

        QStringList sections;
        int previous = -1;
        for (const auto& row : rows) {
            QVERIFY(int(row.id) > previous);  // id order, as Thetis lists them
            previous = int(row.id);
            if (sections.isEmpty() || sections.last() != row.section) { sections << row.section; }
            QVERIFY2(!retiredWords().match(row.caption).hasMatch(), qPrintable(row.caption));
            QVERIFY2(!retiredWords().match(row.tooltip).hasMatch(), qPrintable(row.tooltip));
            QVERIFY(!row.caption.isEmpty());
        }
        QCOMPARE(sections, (QStringList{QStringLiteral("General"), QStringLiteral("Noise / ATT"),
                                        QStringLiteral("Tuning"), QStringLiteral("Display"),
                                        QStringLiteral("Audio / DSP"), QStringLiteral("AGC"),
                                        QStringLiteral("Hardware"), QStringLiteral("RF Power"),
                                        QStringLiteral("Forms"), QStringLiteral("CWX"),
                                        QStringLiteral("Macros")}));

        const auto rowFor = [&rows](Id id) {
            for (const auto& row : rows) {
                if (row.id == id) { return row; }
            }
            return OtherButtonItemEditor::VisibilityRow{id, {}, {}, {}};
        };
        // Split, A > B (VFOs 0 and 1): slices, under Tuning.
        QCOMPARE(rowFor(Id(64)).section, QStringLiteral("Tuning"));
        QCOMPARE(rowFor(Id(65)).tooltip, QStringLiteral("Copy slice A frequency to slice B"));
        QCOMPARE(rowFor(Id::Rx2).caption, QStringLiteral("Slice B"));
        QCOMPARE(rowFor(Id::Rx2).tooltip, QStringLiteral("Enable slice B"));
        QCOMPARE(rowFor(Id::SubRx).tooltip, QStringLiteral("Enable the sub receiver for slice A"));
        QCOMPARE(rowFor(Id::Vac2).caption, QStringLiteral("VAX 2"));
        QCOMPARE(rowFor(Id::Mon).caption, QStringLiteral("MON"));
        QCOMPARE(rowFor(Id::Mon).tooltip, QStringLiteral("Monitor your tx audio"));
    }
};

QTEST_MAIN(TstOtherButtonReorder)
#include "tst_other_button_reorder.moc"
