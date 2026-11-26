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
// directly under it. Where Roundtable can't run, Start and Hide grey with
// the reason; a refused start or hide says why on the line under them.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-11-25: Created. Brian Sutton (W4KZO), AI-assisted via xAI Grok.
//   2026-11-26: The constructor's parent argument; the reason line;
//               greyed with a reason; refusals on the reason line. J.J.
//               Boyd (KG4VCF), AI-assisted via Claude Code / Opus 5.5.
//   2026-11-26: PR 819 final review: the Core's offline and connecting
//               words stay off the reason line; a start it cannot make
//               shows. J.J. Boyd (KG4VCF), AI-assisted via Claude Code /
//               Opus 5.5.
// =================================================================

#include <QtTest/QtTest>

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/SpotSourceHost.h"
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
        applet = std::make_unique<RoundtableControlsApplet>(nullptr, &model, nullptr);
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
    void unavailableGreysBothWithTheReason();
    void aRefusedStartSaysWhy();
    void onlyReasonsItCannotRunShow();
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
    QCOMPARE(labels.size(), 2);
    QCOMPARE(labels.first()->text(), QStringLiteral("Roundtable"));
    QCOMPARE(labels.at(1), applet->reasonLabel());
    QVERIFY(applet->reasonLabel()->isHidden());
    QCOMPARE(buttons.size(), 1);
    QCOMPARE(buttons.first(), applet->startButton());
    QCOMPARE(boxes.size(), 1);
    QCOMPARE(boxes.first()->text(), QStringLiteral("Hide your status"));
    QCOMPARE(applet->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly).size(), 4);
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
    QVERIFY(!f.applet->reasonLabel()->isHidden());
    QCOMPARE(f.applet->reasonLabel()->text(), QStringLiteral("Not now"));
}

void TstRoundtableControls::unavailableGreysBothWithTheReason()
{
    Fixture f;
    const QString why = QStringLiteral("This Core cannot share Roundtable with this app.");
    f.applet->setStationAvailable(false, why);
    QVERIFY(!f.applet->startButton()->isEnabled());
    QVERIFY(!f.applet->hideBox()->isEnabled());
    QCOMPARE(f.applet->startButton()->toolTip(), why);
    QCOMPARE(f.applet->hideBox()->toolTip(), why);

    f.applet->setStationAvailable(true, QString());
    QVERIFY(f.applet->startButton()->isEnabled());
    QVERIFY(f.applet->hideBox()->isEnabled());
    QVERIFY(f.applet->startButton()->toolTip().isEmpty());
    QVERIFY(f.applet->hideBox()->toolTip().isEmpty());
}

void TstRoundtableControls::aRefusedStartSaysWhy()
{
    Fixture f;
    SpotSourceHost host(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    f.applet->setSourceHost(&host);
    QVERIFY(f.applet->reasonLabel()->isHidden());

    const QString why = QStringLiteral("Enter your callsign in Spot Hub first.");
    host.reportStationRefusal(SpotSourceHost::kRoundtable, why);
    QVERIFY(!f.applet->reasonLabel()->isHidden());
    QCOMPARE(f.applet->reasonLabel()->text(), why);

    // Another source's refusal is not Roundtable's.
    host.reportStationRefusal(SpotSourceHost::kPota, QStringLiteral("POTA says no"));
    QCOMPARE(f.applet->reasonLabel()->text(), why);

    // Running again clears it.
    f.state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Live));
    QVERIFY(f.applet->reasonLabel()->isHidden());
}

// A remote window: the Core's Roundtable state and words. Only the words
// of an off Roundtable (a start the Core could not make) are a reason.
void TstRoundtableControls::onlyReasonsItCannotRunShow()
{
    Fixture f;
    SpotSourceHost host(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    host.setStationForwarder([](const QByteArray&, const QString&, const QString&, QString*) {
        return true;
    });
    f.applet->setSourceHost(&host);

    host.applyStationValue("roundtableState", SpotSourceHost::kError);
    host.applyStationValue("roundtableText", QStringLiteral("The Roundtable server is not answering."));
    QVERIFY(f.applet->reasonLabel()->isHidden());

    host.applyStationValue("roundtableState", SpotSourceHost::kConnecting);
    host.applyStationValue("roundtableText", QStringLiteral("Connecting to Roundtable"));
    QVERIFY(f.applet->reasonLabel()->isHidden());

    // Plain stopped: off with nothing to say.
    host.applyStationValue("roundtableState", SpotSourceHost::kOff);
    host.applyStationValue("roundtableText", QString());
    QVERIFY(f.applet->reasonLabel()->isHidden());

    const QString why = QStringLiteral("Enter your callsign in Spot Hub first.");
    host.applyStationValue("roundtableText", why);
    QVERIFY(!f.applet->reasonLabel()->isHidden());
    QCOMPARE(f.applet->reasonLabel()->text(), why);
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
