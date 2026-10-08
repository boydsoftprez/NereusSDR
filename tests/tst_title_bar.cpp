// =================================================================
// tests/tst_title_bar.cpp  (NereusSDR)
// =================================================================
//
// Smoke tests for TitleBar — the 32 px host strip that holds the
// QMenuBar and MasterOutputWidget. Phase 3O Sub-Phase 10 Task 10c.
//
// Coverage:
//   1. constructsWithoutCrash — TitleBar builds against a real
//      AudioEngine without crashing.
//   2. setMenuBarInserts — setMenuBar() actually re-parents the
//      supplied QMenuBar into the strip at position 0.
//   3. masterOutputAccessible — masterOutput() returns the embedded
//      MasterOutputWidget.
//   4. fixedHeight32 — strip height is pinned to 32 px.
//   5. featureButtonEmitsSignal — clicking the 💡 feature button emits
//      featureRequestClicked() exactly once (Task 10d).
//   6. featureButtonShowsBulbIcon: the feature button shows the app's
//      own bulb icon (AppIcon "bulb", R-SPK-19 / D7), not text.
//   7. stripOrderUtcPcRadioFeature: the strip runs UTC, 18 px gap, PC
//      group, 16 px, RADIO group, spacing, feature button (R-SPK-17), and
//      the RADIO group reads as no radio until setRadioModel().
//   8. volumeStacksWhenTheSegmentNeedsTheRoom: with the four remote
//      readouts the header stacks PC above RADIO (layout C) at 1440 px so
//      the segment keeps its width, goes side by side at 2200 px, and
//      comes back each way without flipping in between (R-SPK-17, D1).
//
// Live UI smoke (hosted-inside-QMainWindow + menu bar re-parenting
// visuals) is the canonical verify; these tests only guard the
// construction contract, not appearance.
//
// Design spec: docs/architecture/2026-04-19-vax-design.md §6.3 + §7.3.
// =================================================================

#include <QtTest/QtTest>
#include <QMenu>
#include <QMenuBar>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>

#include "core/AudioEngine.h"
#include "gui/TitleBar.h"
#include "gui/widgets/AppIcon.h"
#include "gui/widgets/MasterOutputWidget.h"
#include "gui/widgets/RadioSpeakerWidget.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TstTitleBar : public QObject {
    Q_OBJECT

private slots:

    // ── 1. Constructs without crashing ─────────────────────────────────────

    void constructsWithoutCrash() {
        AudioEngine engine;
        TitleBar bar(&engine);
        QVERIFY(bar.masterOutput() != nullptr);
    }

    // ── 2. setMenuBar re-parents the menu bar into the strip ──────────────

    void setMenuBarInserts() {
        AudioEngine engine;
        TitleBar bar(&engine);

        auto* mb = new QMenuBar;
        QMenu* dummy = mb->addMenu(QStringLiteral("Dummy"));
        dummy->addAction(QStringLiteral("Nothing"));

        bar.setMenuBar(mb);

        // After setMenuBar() the menu bar must be reachable as a child
        // of the title bar AND parented to it.
        QMenuBar* found = bar.findChild<QMenuBar*>();
        QVERIFY(found != nullptr);
        QCOMPARE(found, mb);
        QCOMPARE(mb->parentWidget(), &bar);
    }

    // ── 3. masterOutput accessor ───────────────────────────────────────────

    void masterOutputAccessible() {
        AudioEngine engine;
        TitleBar bar(&engine);

        MasterOutputWidget* m = bar.masterOutput();
        QVERIFY(m != nullptr);
        // Round-trip check: the widget sits inside the title bar tree.
        QVERIFY(bar.findChild<MasterOutputWidget*>() == m);
    }

    // ── 4. Fixed strip height of 32 px ─────────────────────────────────────

    void fixedHeight32() {
        AudioEngine engine;
        TitleBar bar(&engine);

        // setFixedHeight() pins minimumHeight == maximumHeight == 32. The
        // widget's visible height only resolves after show() / polish, but
        // the height contract is already locked in by setFixedHeight() at
        // construction time and is the canonical check.
        QCOMPARE(bar.minimumHeight(), 32);
        QCOMPARE(bar.maximumHeight(), 32);

        // adjustSize() is enough to resolve the size hint without needing a
        // windowing system (QTEST_MAIN uses a QApplication so this is safe).
        bar.adjustSize();
        QCOMPARE(bar.height(), 32);
    }

    // ── 5. Feature button emits featureRequestClicked ──────────────────────

    void featureButtonEmitsSignal() {
        AudioEngine engine;
        TitleBar bar(&engine);

        // The 💡 button is a QPushButton with objectName "featureButton",
        // set in TitleBar's constructor (Task 10d).
        auto* btn = bar.findChild<QPushButton*>(QStringLiteral("featureButton"));
        QVERIFY(btn != nullptr);

        QSignalSpy spy(&bar, &TitleBar::featureRequestClicked);
        QVERIFY(spy.isValid());

        btn->click();
        QCOMPARE(spy.count(), 1);
    }

    // ── 6. Feature button shows the bulb icon ──────────────────────────────

    void featureButtonShowsBulbIcon() {
        AudioEngine engine;
        TitleBar bar(&engine);

        auto* btn = bar.findChild<QPushButton*>(QStringLiteral("featureButton"));
        QVERIFY(btn != nullptr);
        QCOMPARE(btn->property(AppIcon::kIconProperty).toString(), QStringLiteral("bulb"));
        QVERIFY(!btn->icon().isNull());
        QCOMPARE(btn->iconSize(), QSize(22, 22));
        QVERIFY(btn->text().isEmpty());
    }

    // ── 7. UTC, PC, RADIO, feature button, in that order ───────────────────

    void stripOrderUtcPcRadioFeature() {
        AudioEngine engine;
        TitleBar bar(&engine);
        auto* mb = new QMenuBar;
        mb->addMenu(QStringLiteral("Radio"));
        bar.setMenuBar(mb);

        auto* utc = bar.findChild<QLabel*>(QStringLiteral("utcLabel"));
        MasterOutputWidget* pc = bar.masterOutput();
        RadioSpeakerWidget* radio = bar.radioSpeaker();
        auto* feature = bar.findChild<QPushButton*>(QStringLiteral("featureButton"));
        auto* group = bar.findChild<QWidget*>(QStringLiteral("headerVolumeGroup"));
        QVERIFY(utc && pc && radio && feature && group);
        // PC and RADIO live in one group the strip lays out as a unit, so
        // the header can stack them (R-SPK-17, layout C).
        QCOMPARE(group->parentWidget(), &bar);
        QCOMPARE(pc->parentWidget(), group);
        QCOMPARE(radio->parentWidget(), group);
        QVERIFY(group->layout()->indexOf(pc) < group->layout()->indexOf(radio));

        QLayout* layout = bar.layout();
        const int iUtc = layout->indexOf(utc);
        const int iGroup = layout->indexOf(group);
        const int iFeature = layout->indexOf(feature);
        QVERIFY(iUtc >= 0);
        // One spacer between UTC and the group, and only spacers between
        // the group and the feature button.
        QCOMPARE(iGroup, iUtc + 2);
        QVERIFY(layout->itemAt(iUtc + 1)->spacerItem() != nullptr);
        QVERIFY(iFeature > iGroup + 1);
        for (int i = iGroup + 1; i < iFeature; ++i) {
            QVERIFY(layout->itemAt(i)->spacerItem() != nullptr);
        }
        QCOMPARE(iFeature, layout->count() - 1);

        bar.resize(1440, 32);
        bar.show();
        QApplication::processEvents();
        // No remote readouts: side by side, and the visible gap from PC's
        // readout to RADIO's icon is 16 px.
        QVERIFY(!bar.volumeStacked());
        QCOMPARE(radio->x() - (pc->x() + pc->width()), 16);
        QCOMPARE(radio->y(), pc->y());
        QCOMPARE(bar.height(), 32);
        QVERIFY(group->y() >= 0 && group->y() + group->height() <= 32);

        // Built with no model: disabled, never hidden.
        auto* radioSlider = radio->findChild<QSlider*>(QStringLiteral("radioSlider"));
        QVERIFY(radioSlider);
        QVERIFY(radioSlider->isVisible());
        QVERIFY(!radioSlider->isEnabled());
        QCOMPARE(radioSlider->toolTip(), QStringLiteral("No radio connected"));

        RadioModel model;
        bar.setRadioModel(&model);
        QCOMPARE(radio->radioModel(), &model);
        QVERIFY(!radioSlider->isEnabled());
    }

    // ── 8. Stacked when the connection segment needs the room ─────────────

    void volumeStacksWhenTheSegmentNeedsTheRoom() {
        AudioEngine engine;
        TitleBar bar(&engine);
        auto* menu = new QMenuBar(&bar);
        menu->addMenu(QStringLiteral("Radio"));
        menu->addMenu(QStringLiteral("Setup"));
        menu->addMenu(QStringLiteral("Help"));
        bar.setMenuBar(menu);
        auto* seg = bar.connectionSegment();
        seg->setState(ConnectionState::Connected);
        seg->setRemoteStatusText(QStringLiteral("Core connected"));
        seg->setRemoteMetrics({QStringLiteral("Traffic \u219312.4 \u21910.8 Mbps"),
                               QStringLiteral("Audio 96.0 kbps (playing)"),
                               QStringLiteral("Radio \u219312.4 \u21910.8 Mbps"),
                               QStringLiteral("Core RTT 18 ms")});
        bar.resize(1440, 32);
        bar.show();
        QApplication::processEvents();

        MasterOutputWidget* pc = bar.masterOutput();
        RadioSpeakerWidget* radio = bar.radioSpeaker();
        const QFontMetrics metrics(QFont(QStringLiteral("SF Mono"), 10, QFont::DemiBold));
        const int text = metrics.horizontalAdvance(seg->remotePresentationText());
        auto expectStacked = [&]() {
            QVERIFY(bar.volumeStacked());
            QVERIFY(pc->isStacked());
            QVERIFY(radio->isStacked());
            QVERIFY(pc->y() + pc->height() <= radio->y());
            QVERIFY(seg->width() >= text + 34);
        };
        expectStacked();
        // Same children in both forms, all still shown.
        for (const char* name : {"speakerBtn", "pcLabel", "masterSlider", "dbLabel",
                                 "radioSpeakerBtn", "radioLabel", "radioSlider",
                                 "radioValueLabel"}) {
            auto* child = bar.findChild<QWidget*>(QLatin1String(name));
            QVERIFY2(child, name);
            QVERIFY2(child->isVisible(), name);
            const QPoint top = child->mapTo(&bar, QPoint(0, 0));
            QVERIFY2(top.y() >= 0 && top.y() + child->height() <= 32, name);
        }
        QCOMPARE(bar.height(), 32);

        // Readings come every second; the form holds.
        for (int i = 0; i < 5; ++i) {
            seg->setRemoteMetrics({QStringLiteral("Traffic \u2193%1.4 \u21910.8 Mbps").arg(10 + i),
                                   QStringLiteral("Audio 96.0 kbps (playing)"),
                                   QStringLiteral("Radio \u219312.4 \u21910.8 Mbps"),
                                   QStringLiteral("Core RTT 18 ms")});
            QApplication::processEvents();
            QVERIFY(bar.volumeStacked());
        }

        bar.resize(2200, 32);
        QApplication::processEvents();
        QVERIFY(!bar.volumeStacked());
        QVERIFY(!pc->isStacked());
        QVERIFY(!radio->isStacked());
        QCOMPARE(radio->y(), pc->y());
        QCOMPARE(radio->x() - (pc->x() + pc->width()), 16);
        QVERIFY(seg->width() >= metrics.horizontalAdvance(seg->remotePresentationText()) + 34);
        QCOMPARE(bar.height(), 32);

        bar.resize(1440, 32);
        QApplication::processEvents();
        expectStacked();
    }
};

QTEST_MAIN(TstTitleBar)
#include "tst_title_bar.moc"
