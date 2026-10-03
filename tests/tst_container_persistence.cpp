// tst_container_persistence.cpp — meter items inside containers must
// survive a ContainerManager save/restore round-trip.
//
// RED test: with current code, ContainerWidget::serialize() persists
// 25 fields of metadata but never serializes its inner MeterWidget's
// items, and ContainerManager::restoreState never materializes a
// MeterWidget for restored containers (the constructor only installs
// a placeholder QLabel). After restart, user-created containers come
// back empty.

#include <QtTest/QtTest>
#include <QSplitter>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#include "core/AppSettings.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/meters/AntennaButtonItem.h"
#include "gui/meters/BandButtonItem.h"
#include "gui/meters/FilterButtonItem.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterWidget.h"
#include "gui/meters/ModeButtonItem.h"
#include "gui/meters/OtherButtonItem.h"
#include "gui/meters/TuneStepButtonItem.h"
#include "gui/meters/VfoDisplayItem.h"

#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSet>
#include <QStackedWidget>

using namespace NereusSDR;

class TstContainerPersistence : public QObject
{
    Q_OBJECT

private:
    static void clearContainerKeys()
    {
        auto& s = AppSettings::instance();
        const QStringList keys = s.allKeys();
        for (const QString& k : keys) {
            if (k.startsWith(QStringLiteral("Container"))) {
                s.remove(k);
            }
        }
    }

private slots:
    void init() { clearContainerKeys(); }
    void cleanup() { clearContainerKeys(); }

    void userContainerItemsSurviveSaveRestore()
    {
        QWidget dockParent;
        QSplitter splitter;
        QString savedId;

        // --- Phase 1: create a user container, populate, save ---
        {
            ContainerManager mgr(&dockParent, &splitter);
            ContainerWidget* c = mgr.createContainer(1, DockMode::Floating);
            QVERIFY(c);
            savedId = c->id();

            auto* meter = new MeterWidget();
            c->setContent(meter);

            meter->addItem(new BarItem());
            meter->addItem(new TextItem());
            QCOMPARE(meter->items().size(), 2);

            mgr.saveState();
        }

        // --- Phase 2: fresh manager, restore, verify items survived ---
        {
            ContainerManager mgr2(&dockParent, &splitter);
            mgr2.restoreState();

            ContainerWidget* c = mgr2.container(savedId);
            QVERIFY2(c != nullptr, "container missing after restoreState");

            auto* meter = qobject_cast<MeterWidget*>(c->content());
            QVERIFY2(meter != nullptr,
                     "restored container has no MeterWidget content "
                     "(ContainerManager::restoreState did not materialize one)");
            QCOMPARE(meter->items().size(), 2);
        }
    }

    // Container #0 wraps its MeterWidget inside an AppletPanelWidget
    // (header slot). innerMeterWidget() must find the nested meter via
    // findChild so save/restore works for the wrapped shape too. We
    // simulate the wrap with a plain QWidget hosting a MeterWidget
    // child — same QObject parent/child relationship the real
    // AppletPanelWidget creates.
    void wrappedMeterItemsSurviveSaveRestore()
    {
        QWidget dockParent;
        QSplitter splitter;
        QString savedId;

        {
            ContainerManager mgr(&dockParent, &splitter);
            mgr.setContentFactory([](const QString&, int) -> QWidget* {
                auto* wrapper = new QWidget();
                auto* layout = new QVBoxLayout(wrapper);
                layout->setContentsMargins(0, 0, 0, 0);
                auto* meter = new MeterWidget();
                layout->addWidget(meter);
                return wrapper;
            });

            ContainerWidget* c = mgr.createContainer(0, DockMode::PanelDocked);
            QVERIFY(c);
            savedId = c->id();

            // Build the wrap shape and install it as content.
            auto* wrapper = new QWidget();
            auto* layout = new QVBoxLayout(wrapper);
            layout->setContentsMargins(0, 0, 0, 0);
            auto* meter = new MeterWidget(wrapper);
            layout->addWidget(meter);
            c->setContent(wrapper);

            meter->addItem(new BarItem());
            meter->addItem(new TextItem());
            meter->addItem(new BarItem());
            QCOMPARE(meter->items().size(), 3);

            mgr.saveState();
        }

        {
            ContainerManager mgr2(&dockParent, &splitter);
            mgr2.setContentFactory([](const QString&, int) -> QWidget* {
                auto* wrapper = new QWidget();
                auto* layout = new QVBoxLayout(wrapper);
                layout->setContentsMargins(0, 0, 0, 0);
                auto* meter = new MeterWidget();
                layout->addWidget(meter);
                return wrapper;
            });
            mgr2.restoreState();

            ContainerWidget* c = mgr2.container(savedId);
            QVERIFY(c);

            // The container content is the wrapper QWidget, not the
            // meter directly — exercises findChild<MeterWidget*>().
            auto* meter = c->content() ? c->content()->findChild<MeterWidget*>() : nullptr;
            QVERIFY2(meter != nullptr, "wrapped MeterWidget not found after restore");
            QCOMPARE(meter->items().size(), 3);
        }
    }

    // ContainerManager must announce every MeterWidget it manages so
    // MainWindow can wire them into MeterPoller. Previously, only the
    // initial m_meterWidget on the panel container was registered;
    // user-created containers' meters were orphaned and never received
    // setValue() calls — bars sat at frac=0 (visually invisible) and
    // needles never moved. Symptom: "BarMeter not drawing".
    void createdContainerMeterAnnouncedForPolling()
    {
        QWidget dockParent;
        QSplitter splitter;
        ContainerManager mgr(&dockParent, &splitter);

        QList<MeterWidget*> announced;
        QObject::connect(&mgr, &ContainerManager::meterReadyForPolling,
                         [&announced](MeterWidget* m) {
            announced.append(m);
        });

        ContainerWidget* c = mgr.createContainer(1, DockMode::Floating);
        QVERIFY(c);
        auto* meter = new MeterWidget();
        c->setContent(meter);

        QCOMPARE(announced.size(), 1);
        QCOMPARE(announced.first(), meter);
    }

    // After restoreState, every container that ContainerManager
    // materialized a MeterWidget for must also be announced. The
    // restored meter is a fresh instance, not the one the test
    // populated in phase 1, so we just assert that exactly one signal
    // fired with a non-null pointer matching the container's content.
    void restoredContainerMeterAnnouncedForPolling()
    {
        QWidget dockParent;
        QSplitter splitter;
        QString savedId;

        {
            ContainerManager mgr(&dockParent, &splitter);
            ContainerWidget* c = mgr.createContainer(1, DockMode::Floating);
            savedId = c->id();
            auto* meter = new MeterWidget();
            c->setContent(meter);
            meter->addItem(new BarItem());
            mgr.saveState();
        }

        {
            ContainerManager mgr2(&dockParent, &splitter);

            QList<MeterWidget*> announced;
            QObject::connect(&mgr2, &ContainerManager::meterReadyForPolling,
                             [&announced](MeterWidget* m) {
                announced.append(m);
            });

            mgr2.restoreState();

            QCOMPARE(announced.size(), 1);
            QVERIFY(announced.first() != nullptr);

            ContainerWidget* c = mgr2.container(savedId);
            QVERIFY(c);
            QCOMPARE(announced.first(), qobject_cast<MeterWidget*>(c->content()));
        }
    }

    // NeedleItem::setValue historically clamped to the AetherSDR
    // S-meter dBm range [-127, -13]. ANANMM's calibrated needles
    // (Volts 10-15, Amps 0-20, Power 0-150W, SWR 1-10, Compression
    // 0-30 dB, ALC 0-30 dB) all use NATIVE units in their calibration
    // maps. The dBm clamp destroyed the value before
    // calibratedPosition() ran, collapsing every calibrated needle
    // to the first/last calibration point. Visually: needles drew as
    // a near-horizontal line at the offset row regardless of value.
    //
    // RED: with the old clamp, smoothedValue() will sit at -13.0 (or
    // similar) after pushing 13.0 because it's outside [-127, -13].
    // GREEN: clamp uses calibration map's key range when the map is
    // populated.
    void calibratedNeedleNotClampedToDbmRange()
    {
        NeedleItem needle;

        // Volts-style calibration: keys 10..15 V, all positions in
        // the same y row (matches the ANANMM voltmeter layout).
        QMap<float, QPointF> cal;
        cal.insert(10.0f, QPointF(0.559, 0.756));
        cal.insert(12.5f, QPointF(0.605, 0.772));
        cal.insert(15.0f, QPointF(0.665, 0.784));
        needle.setScaleCalibration(cal);

        // Push a realistic Volts reading enough times for the
        // exponential smoothing (alpha 0.3) to converge from the
        // default kS0Dbm.
        for (int i = 0; i < 40; ++i) {
            needle.setValue(13.0);
        }

        // Must be inside the calibration range, NOT clamped to the
        // dBm range (-127..-13) which would leave it at or below -13.
        const float v = needle.smoothedValue();
        QVERIFY2(v >= 10.0f,
                 qPrintable(QStringLiteral(
                     "calibrated needle smoothedValue %1 below "
                     "calibration min — dBm clamp not bypassed").arg(v)));
        QVERIFY2(v <= 15.0f,
                 qPrintable(QStringLiteral(
                     "calibrated needle smoothedValue %1 above "
                     "calibration max").arg(v)));
    }

    // ContainerSettingsDialog::addNewItem stacks new items vertically
    // by computing yPos = max(y + height) of existing items, clamped
    // at 0.9. The ANANMM preset installs 7 needles each at (0,0,1,1)
    // — full-container background overlays. With the old logic, the
    // clamp pinned every new item at y=0.9 and they piled up on top
    // of each other. The fix: items with itemHeight() > 0.7 are
    // treated as backgrounds and excluded from the stack
    // calculation.
    void nextStackYPosIgnoresFullContainerOverlays()
    {
        // Case 1: only an ANANMM-style full-container needle present.
        // Old behaviour returned 0.9 (clamped from 1.0). New
        // behaviour should return 0.0 — there are no narrow stacked
        // items, so the next item starts at the top.
        QVector<MeterItem*> overlaysOnly;
        auto* fullNeedle = new NeedleItem();
        fullNeedle->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        overlaysOnly.append(fullNeedle);

        const float yOverlay =
            ContainerSettingsDialog::nextStackYPos(overlaysOnly);
        QVERIFY2(yOverlay < 0.05f,
                 qPrintable(QStringLiteral(
                     "yPos %1 still pinned by full-container overlay").arg(yOverlay)));

        // Case 2: full-container background + two real stacked bars.
        // Should return 0.30 (after the second bar), not 0.9.
        QVector<MeterItem*> mixed;
        auto* bg = new NeedleItem();
        bg->setRect(0.0f, 0.0f, 1.0f, 1.0f);
        mixed.append(bg);

        auto* bar1 = new BarItem();
        bar1->setRect(0.0f, 0.0f, 1.0f, 0.15f);
        mixed.append(bar1);

        auto* bar2 = new BarItem();
        bar2->setRect(0.0f, 0.15f, 1.0f, 0.15f);
        mixed.append(bar2);

        const float yMixed =
            ContainerSettingsDialog::nextStackYPos(mixed);
        QVERIFY2(qFuzzyCompare(yMixed + 1.0f, 0.30f + 1.0f),
                 qPrintable(QStringLiteral(
                     "yPos %1 should follow stacked bars (0.30), not "
                     "the overlay clamp").arg(yMixed)));

        qDeleteAll(overlaysOnly);
        qDeleteAll(mixed);
    }

    // ── R3 unfinished controls, Task 3 (R-R3-49, R-R3-21) ────────────────

    // A layout saved with Thetis's RX1 / RX2 receiver field loads as slice
    // A / B; the choice offers slices A to D and round-trips.
    void savedReceiverLoadsAsASliceAndRoundTrips()
    {
        QWidget dockParent;
        QSplitter splitter;
        QString savedId;
        {
            ContainerManager mgr(&dockParent, &splitter);
            ContainerWidget* c = mgr.createContainer(1, DockMode::Floating);
            QVERIFY(c);
            savedId = c->id();
            c->setContent(new MeterWidget());
            // Today's saved form: field 1 is the RX number.
            QStringList fields = c->serialize().split(QLatin1Char('|'));
            fields[1] = QStringLiteral("2");
            QVERIFY(c->deserialize(fields.join(QLatin1Char('|'))));
            QCOMPARE(c->rxSource(), 2);
            mgr.saveState();
        }
        ContainerManager mgr(&dockParent, &splitter);
        mgr.restoreState();
        ContainerWidget* c = mgr.container(savedId);
        QVERIFY(c);
        QCOMPARE(c->rxSource(), 2);
        bool titled = false;
        for (QLabel* label : c->findChildren<QLabel*>()) {
            QVERIFY2(!label->text().contains(QStringLiteral("RX2")), qPrintable(label->text()));
            if (label->text() == QStringLiteral("Slice B")) { titled = true; }
        }
        QVERIFY(titled);

        ContainerSettingsDialog dialog(c);
        QComboBox* slices = nullptr;
        for (QComboBox* combo : dialog.findChildren<QComboBox*>()) {
            if (combo->findText(QStringLiteral("Slice A")) >= 0) { slices = combo; }
        }
        QVERIFY(slices != nullptr);
        QCOMPARE(slices->count(), 4);
        for (int i = 0; i < 4; ++i) {
            QCOMPARE(slices->itemText(i), ContainerWidget::sliceNameForRxSource(i + 1));
            QCOMPARE(slices->itemData(i).toInt(), i + 1);
        }
        QCOMPARE(slices->currentText(), QStringLiteral("Slice B"));
        QCOMPARE(slices->findText(QStringLiteral("RX1")), -1);

        QSignalSpy changed(c, &ContainerWidget::rxSourceChanged);
        slices->setCurrentIndex(3);
        QPushButton* apply = nullptr;
        for (QPushButton* button : dialog.findChildren<QPushButton*>()) {
            if (button->text() == QStringLiteral("Apply")) { apply = button; }
        }
        QVERIFY(apply != nullptr);
        apply->click();
        QCOMPARE(c->rxSource(), 4);
        QCOMPARE(changed.count(), 1);

        ContainerWidget copy;
        QVERIFY(copy.deserialize(c->serialize()));
        QCOMPARE(copy.rxSource(), 4);
    }

    // Fix wave M5 (R-R3-49, R-R3-21): a receiver value outside slices A to
    // D (0, or above 4, from a hand-edited or damaged layout) loads as the
    // nearest slice, so the title and the buttons' reason name a slice.
    void outOfRangeSavedReceiverLoadsAsTheNearestSlice()
    {
        const QList<QPair<QString, int>> cases{
            {QStringLiteral("0"), 1}, {QStringLiteral("-3"), 1},
            {QStringLiteral("5"), 4}, {QStringLiteral("27"), 4}};
        for (const auto& [saved, slice] : cases) {
            ContainerWidget source;
            QStringList fields = source.serialize().split(QLatin1Char('|'));
            fields[1] = saved;
            ContainerWidget c;
            QVERIFY2(c.deserialize(fields.join(QLatin1Char('|'))), qPrintable(saved));
            QCOMPARE(c.rxSource(), slice);
            bool titled = false;
            for (QLabel* label : c.findChildren<QLabel*>()) {
                if (label->text() == ContainerWidget::sliceNameForRxSource(slice)) { titled = true; }
                QVERIFY2(label->text() != QStringLiteral("Slice"), qPrintable(saved));
            }
            QVERIFY2(titled, qPrintable(saved));
        }
    }

    // A layout saved today with every function button visible loads with
    // the buttons that have no feature not drawn, and saving it keeps
    // their saved visibility unchanged. Its Power button (bit 0) is
    // dropped: NereusSDR has no Power button (maintainer decision
    // 2026-09-30). The load succeeds and every other button is intact.
    void everyButtonVisibleLayoutKeepsItsSavedVisibility()
    {
        QWidget dockParent;
        QSplitter splitter;
        QString savedId;
        const QString savedItem = QStringLiteral("OTHERBTNS|0|0|1|0.5|0|10|6|4294967295");
        const QString savedWithoutPower = QStringLiteral("OTHERBTNS|0|0|1|0.5|0|10|6|4294967294");
        {
            ContainerManager mgr(&dockParent, &splitter);
            ContainerWidget* c = mgr.createContainer(1, DockMode::Floating);
            QVERIFY(c);
            savedId = c->id();
            auto* meter = new MeterWidget();
            c->setContent(meter);
            auto* item = new OtherButtonItem();
            QVERIFY(item->deserialize(savedItem));
            meter->addItem(item);
            mgr.saveState();
        }
        for (int round = 0; round < 2; ++round) {
            ContainerManager mgr(&dockParent, &splitter);
            mgr.restoreState();
            ContainerWidget* c = mgr.container(savedId);
            QVERIFY(c);
            auto* meter = qobject_cast<MeterWidget*>(c->content());
            QVERIFY(meter);
            QCOMPARE(meter->items().size(), 1);
            auto* item = qobject_cast<OtherButtonItem*>(meter->items().first());
            QVERIFY(item);
            QCOMPARE(item->visibleBits(), 0xFFFFFFFEu);
            QVERIFY(!item->isButtonShown(OtherButtonItem::ButtonId::Power));
            for (auto id : {OtherButtonItem::ButtonId::Mon, OtherButtonItem::ButtonId::Tun,
                            OtherButtonItem::ButtonId::Mox, OtherButtonItem::ButtonId::TwoTon,
                            OtherButtonItem::ButtonId::PsA, OtherButtonItem::ButtonId::Mute}) {
                QVERIFY(item->isButtonShown(id));
            }
            QVERIFY(item->isButtonShown(OtherButtonItem::ButtonId::Anf));
            QVERIFY(item->isButtonShown(OtherButtonItem::ButtonId::Vac1));
            // Parity Task 31: DUP is built (display duplex).
            QVERIFY(item->isButtonShown(OtherButtonItem::ButtonId::Dup));
            for (auto id : {OtherButtonItem::ButtonId::Rx2,
                            OtherButtonItem::ButtonId::Play, OtherButtonItem::ButtonId::Rec,
                            OtherButtonItem::ButtonId::Xpa, OtherButtonItem::ButtonId::Avg,
                            OtherButtonItem::ButtonId::Waterfall,
                            OtherButtonItem::ButtonId::DisplayOff}) {
                QVERIFY(!item->isButtonShown(id));
            }
            QCOMPARE(item->serialize(), savedWithoutPower);
            mgr.saveState();
        }
    }

    // The property editor opens for the function buttons and the band,
    // mode, filter, antenna, tune step and VFO display items: each tag it
    // looks for is the one the item saves.
    void itemEditorOpensForButtonBoxesAndTheVfoDisplay()
    {
        ContainerWidget container;
        auto* meter = new MeterWidget();
        container.setContent(meter);
        meter->addItem(new OtherButtonItem());
        meter->addItem(new BandButtonItem());
        meter->addItem(new ModeButtonItem());
        meter->addItem(new FilterButtonItem());
        meter->addItem(new AntennaButtonItem());
        meter->addItem(new TuneStepButtonItem());
        meter->addItem(new VfoDisplayItem());

        ContainerSettingsDialog dialog(&container);
        QListWidget* list = nullptr;
        for (QListWidget* candidate : dialog.findChildren<QListWidget*>()) {
            if (candidate->count() == 7) { list = candidate; }
        }
        QVERIFY(list != nullptr);
        QStackedWidget* stack = dialog.findChild<QStackedWidget*>();
        QVERIFY(stack != nullptr);

        QSet<QString> opened;
        for (int row = 0; row < list->count(); ++row) {
            list->setCurrentRow(row);
            auto* scroll = qobject_cast<QScrollArea*>(stack->currentWidget());
            QVERIFY2(scroll != nullptr && scroll->widget() != nullptr,
                     qPrintable(QStringLiteral("no editor for %1")
                                    .arg(list->item(row)->text())));
            opened.insert(QString::fromLatin1(scroll->widget()->metaObject()->className()));
        }
        const QSet<QString> expected = {
            QStringLiteral("NereusSDR::OtherButtonItemEditor"),
            QStringLiteral("NereusSDR::BandButtonItemEditor"),
            QStringLiteral("NereusSDR::ModeButtonItemEditor"),
            QStringLiteral("NereusSDR::FilterButtonItemEditor"),
            QStringLiteral("NereusSDR::AntennaButtonItemEditor"),
            QStringLiteral("NereusSDR::TuneStepButtonItemEditor"),
            QStringLiteral("NereusSDR::VfoDisplayItemEditor"),
        };
        QCOMPARE(opened, expected);
    }
};

QTEST_MAIN(TstContainerPersistence)
#include "tst_container_persistence.moc"
