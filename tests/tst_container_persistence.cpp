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
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterWidget.h"

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
};

QTEST_MAIN(TstContainerPersistence)
#include "tst_container_persistence.moc"
