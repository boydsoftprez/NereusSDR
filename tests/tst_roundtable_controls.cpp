// no-port-check: NereusSDR-original.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// tests/tst_roundtable_controls.cpp  (NereusSDR)
// =================================================================
//
// The Roundtable controls applet: a Roundtable label, a Start/Stop button,
// and Hide your status. Nothing else. Start and Stop are the Spot Hub
// signals. Hide your status is the model's setHidden, and a refusal puts
// the box back. A workspace that already has Roundtable gains this panel
// directly under it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-25: Created. Brian Sutton (W4KZO), AI-assisted via xAI Grok.
// =================================================================

#include <QtTest/QtTest>

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/roundtable/RoundtableState.h"
#include "core/session/IStationLink.h"
#include "gui/applets/AppletVisibilityController.h"
#include "gui/applets/RoundtableControlsApplet.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/styles/WorkspacePreview.h"
#include "models/RoundtableModel.h"

using namespace NereusSDR;

namespace {

class RecordingLink : public IStationLink {
public:
    CommandOutcome requestAddSlice(const QString&) override { return {}; }
    CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
    CommandOutcome requestRemoveSlice(int) override { return {}; }
    CommandOutcome requestActiveSlice(int) override { return {}; }
    CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
    CommandOutcome requestRoundtableSetHidden(bool hidden) override
    {
        asks.append(hidden);
        return {accept, reason};
    }

    QList<bool> asks;
    bool accept = true;
    QString reason;
};

struct Fixture {
    RecordingLink link;
    RoundtableState state;
    RoundtableModel model;
    std::unique_ptr<RoundtableControlsApplet> applet;

    Fixture()
    {
        model.attachStation(&link, &state);
        applet = std::make_unique<RoundtableControlsApplet>(nullptr, &model);
    }
};

ContentEntry savedApplet(const QString& type)
{
    ContentEntry entry;
    entry.id = type + QStringLiteral("-id");
    entry.typeId = type;
    entry.name = type;
    entry.visible = true;
    return entry;
}

} // namespace

class TstRoundtableControls : public QObject {
    Q_OBJECT
private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void panelHasOnlyTheTwoControls();
    void startAndStopFollowTheLink();
    void hideYourStatusWritesAndFollowsTheCore();
    void aRefusedHidePutsTheBoxBack();
    void anExistingWorkspaceGainsItUnderRoundtable();
    void firstLaunchPlacesItUnderRoundtable();
};

void TstRoundtableControls::panelHasOnlyTheTwoControls()
{
    Fixture f;
    auto* applet = f.applet.get();
    const QList<QLabel*> labels = applet->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly);
    const QList<QPushButton*> buttons =
        applet->findChildren<QPushButton*>(QString(), Qt::FindDirectChildrenOnly);
    const QList<QCheckBox*> boxes = applet->findChildren<QCheckBox*>(QString(), Qt::FindDirectChildrenOnly);
    QCOMPARE(labels.size(), 1);
    QCOMPARE(labels.first()->text(), QStringLiteral("Roundtable"));
    QCOMPARE(buttons.size(), 1);
    QCOMPARE(buttons.first(), applet->startButton());
    QCOMPARE(boxes.size(), 1);
    QCOMPARE(boxes.first()->text(), QStringLiteral("Hide your status"));
    QCOMPARE(applet->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly).size(), 3);
}

void TstRoundtableControls::startAndStopFollowTheLink()
{
    Fixture f;
    QPushButton* button = f.applet->startButton();
    QSignalSpy started(f.applet.get(), &RoundtableControlsApplet::startRequested);
    QSignalSpy stopped(f.applet.get(), &RoundtableControlsApplet::stopRequested);
    QCOMPARE(button->text(), QStringLiteral("Start"));

    button->click();
    QCOMPARE(started.count(), 1);
    QCOMPARE(stopped.count(), 0);

    f.state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Connecting));
    QCOMPARE(button->text(), QStringLiteral("Stop"));
    f.state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Live));
    QCOMPARE(button->text(), QStringLiteral("Stop"));

    button->click();
    QCOMPARE(started.count(), 1);
    QCOMPARE(stopped.count(), 1);

    f.state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Off));
    QCOMPARE(button->text(), QStringLiteral("Start"));
}

void TstRoundtableControls::hideYourStatusWritesAndFollowsTheCore()
{
    Fixture f;
    QCheckBox* hide = f.applet->hideBox();
    QVERIFY(!hide->isChecked());

    hide->click();
    QCOMPARE(f.link.asks, QList<bool>{true});
    QVERIFY(hide->isChecked());

    f.state.applyStationValue("hidden", true);
    QVERIFY(hide->isChecked());
    f.state.applyStationValue("hidden", false);
    QVERIFY(!hide->isChecked());
    QCOMPARE(f.link.asks, QList<bool>{true});
}

void TstRoundtableControls::aRefusedHidePutsTheBoxBack()
{
    Fixture f;
    f.link.accept = false;
    f.link.reason = QStringLiteral("Not now");
    QSignalSpy refused(&f.model, &RoundtableModel::actionRefused);

    f.applet->hideBox()->click();
    QCOMPARE(f.link.asks, QList<bool>{true});
    QCOMPARE(refused.count(), 1);
    QCOMPARE(refused.first().at(0).toString(), QStringLiteral("setHidden"));
    QVERIFY(!f.applet->hideBox()->isChecked());
}

void TstRoundtableControls::anExistingWorkspaceGainsItUnderRoundtable()
{
    WorkspacePreview::setEnabledForTest(false);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    AppSettings settings(dir.filePath(QStringLiteral("settings.xml")));
    WorkspaceDocument doc;
    ContainerDocument main;
    main.id = QStringLiteral("main");
    main.name = QStringLiteral("Main Panel");
    main.layout = ContentLayout::VerticalStack;
    main.contents = {savedApplet(QStringLiteral("applet:rx")),
                     savedApplet(QStringLiteral("applet:roundtable"))};
    doc.mainContainerId = main.id;
    doc.containers = {main};
    settings.setValue(QStringLiteral("ContainerWorkspace"),
                      QString::fromUtf8(ContainerDocumentCodec::encode(doc)));
    QVERIFY(settings.save());

    ContainerWorkspaceStore store(settings);
    QVERIFY(store.loadError().isEmpty());
    ContainerContentRegistry registry;
    AppletVisibilityController controller;
    controller.setWorkspaceAdapter(&store, &registry);
    controller.registerApplet(QStringLiteral("Roundtable"), QStringLiteral("Roundtable"), true);
    controller.registerApplet(QStringLiteral("RoundtableControls"),
                              QStringLiteral("Roundtable controls (simple)"), true);

    const QVector<ContentEntry>& contents = store.snapshot().containers.first().contents;
    QCOMPARE(contents.size(), 3);
    QCOMPARE(contents.at(1).typeId, QStringLiteral("applet:roundtable"));
    QCOMPARE(contents.at(2).typeId, QStringLiteral("applet:roundtable_controls"));
    QCOMPARE(contents.at(2).name, QStringLiteral("Roundtable controls (simple)"));
    QVERIFY(contents.at(2).visible);
    QVERIFY(controller.isVisible(QStringLiteral("RoundtableControls")));
}

void TstRoundtableControls::firstLaunchPlacesItUnderRoundtable()
{
    ContainerContentRegistry registry;
    QVector<ContentEntry> contents;
    for (const QString& type : {QStringLiteral("applet:rx"), QStringLiteral("applet:roundtable"),
                                QStringLiteral("applet:roundtable_controls"),
                                QStringLiteral("applet:s_meter")}) {
        ContainerContentRegistry::placeFirstLaunchApplet(
            contents, ContainerContentRegistry::firstLaunchEntry(registry.makeEntry(type)));
    }
    int roundtable = -1;
    int controls = -1;
    for (int i = 0; i < contents.size(); ++i) {
        if (contents[i].typeId == QLatin1String("applet:roundtable")) { roundtable = i; }
        if (contents[i].typeId == QLatin1String("applet:roundtable_controls")) { controls = i; }
    }
    QVERIFY(roundtable >= 0);
    QCOMPARE(controls, roundtable + 1);
    QVERIFY(contents.at(controls).visible);
}

QTEST_MAIN(TstRoundtableControls)
#include "tst_roundtable_controls.moc"
