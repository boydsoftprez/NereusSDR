// =================================================================
// tests/tst_slice_chooser.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. The bottom RX area's all-slice
// chooser (slice control and shared listening plan Task 13): its rows and
// actions in each state, a request waiting for the Core's answer, the
// empty window's honest words, narrow widths, the two row builders and two
// same-named devices told apart. Renders every state offscreen
// (NEREUS_TASK78_SHOTS keeps them); nothing opens a window.
//
// Modification history (NereusSDR):
//   2026-09-29: created for NereusSDR by J.J. Boyd (KG4VCF), slice control
//               and shared listening plan Task 13, with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: core-slice take-over: Take control of the Core's own
//               slice is disabled with the Core's words below
//               sliceAccessVersion 3. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: take-over review: the Core's own slice reads "the Core
//               itself". J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: desktop listening lane: the hosting desktop by its own
//               name, a headless Core as the Core itself, the station
//               marker, and a slice kept for an away device. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "MultiDeviceHarness.h"

#include <QDialog>
#include <QDir>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSignalSpy>

#include "core/session/RemoteDevicesState.h"
#include "core/session/SliceAccessMirror.h"
#include "gui/SliceChooser.h"
#include "gui/SpectrumWidget.h"
#include "gui/StyleConstants.h"
#include "gui/multidevice/MultiDeviceController.h"
#include "gui/widgets/RxDashboard.h"
#include "gui/widgets/VfoWidget.h"

using namespace NereusSDR;

namespace {

using Row = SliceChooser::Row;
using Controller = SliceChooser::Controller;

Row row(int id, Controller controller, const QString& name = QString())
{
    Row r;
    r.sliceId = id;
    r.color = VfoWidget::sliceColor(id);
    r.frequencyHz = 14200000.0 + id * 10000.0;
    r.mode = QStringLiteral("USB");
    r.filter = QStringLiteral("2.4k");
    r.controller = controller;
    r.controllerName = name;
    r.listenerCount = controller == Controller::Nobody ? 0 : 1;
    return r;
}

QStringList actionWords(const SliceChooser& chooser)
{
    QStringList words;
    for (QPushButton* b : chooser.findChildren<QPushButton*>(QStringLiteral("sliceChooserAction"))) {
        if (!b->isHidden() && b->parent() != nullptr) {
            words.append(b->text());
        }
    }
    return words;
}

QPushButton* action(const SliceChooser& chooser, const QString& words)
{
    for (QPushButton* b : chooser.findChildren<QPushButton*>(QStringLiteral("sliceChooserAction"))) {
        if (b->text() == words) {
            return b;
        }
    }
    return nullptr;
}

// The bottom area as it looks: the dashboard's picker under the open
// chooser, at the chooser's width.
void render(SliceChooser& chooser, const QString& banner, const QString& name)
{
    QCoreApplication::processEvents();
    chooser.adjustSize();
    chooser.resize(std::max(chooser.sizeHint().width(), 420), chooser.sizeHint().height());
    RxDashboard dashboard;
    dashboard.setAutoFillBackground(true);
    QPalette dark = dashboard.palette();
    dark.setColor(QPalette::Window, QColor(Style::kAppBg));
    dashboard.setPalette(dark);
    dashboard.setChooserState(banner);
    dashboard.adjustSize();
    const QImage top = chooser.grab().toImage();
    const QImage bottom = dashboard.grab().toImage();
    QVERIFY(!top.isNull() && !bottom.isNull());
    const QString dir = qEnvironmentVariable("NEREUS_TASK78_SHOTS");
    if (dir.isEmpty()) {
        return;
    }
    QImage out(std::max(top.width(), bottom.width()), top.height() + bottom.height() + 6,
               QImage::Format_ARGB32);
    out.fill(QColor(Style::kAppBg));
    QPainter painter(&out);
    painter.drawImage(0, 0, top);
    painter.drawImage(0, top.height() + 6, bottom);
    painter.end();
    QDir().mkpath(dir);
    QVERIFY(out.save(QDir(dir).filePath(QStringLiteral("slice-chooser-%1.png").arg(name))));
}

MirrorUpdate text(const char* name, const QString& value)
{
    return MirrorUpdate{0, name, MirrorWireKind::Utf8, QVariant(value)};
}

MirrorUpdate count(const char* name, qint64 value)
{
    return MirrorUpdate{0, name, MirrorWireKind::Int64, QVariant(value)};
}

MirrorUpdate flag(const char* name, bool value)
{
    return MirrorUpdate{0, name, MirrorWireKind::Bool, QVariant(value)};
}

} // namespace

class TstSliceChooser : public QObject {
    Q_OBJECT

private slots:
    // Another device's slice: listen in, or take control of it.
    void anotherDevicesSliceOffersListenAndTake()
    {
        SliceChooser chooser;
        chooser.setInventory({row(0, Controller::OtherDevice, QStringLiteral("Jo's iPhone")),
                              [] { Row r = row(1, Controller::ThisWindow);
                                   r.listeningHere = true; r.activeHere = true; return r; }()});
        chooser.selectSlice(0);
        QCOMPARE(actionWords(chooser), (QStringList{QStringLiteral("Listen in"),
                                                    QStringLiteral("Take control")}));
        QSignalSpy listen(&chooser, &SliceChooser::listenRequested);
        QSignalSpy take(&chooser, &SliceChooser::takeControlRequested);
        action(chooser, QStringLiteral("Listen in"))->click();
        QCOMPARE(listen.size(), 1);
        QCOMPARE(listen.first().at(0).toInt(), 0);
        action(chooser, QStringLiteral("Take control"))->click();
        QCOMPARE(take.size(), 1);
        render(chooser, QStringLiteral("You control"), QStringLiteral("remote-another-device"));
    }

    // A slice this window listens to: select it, take control, or stop.
    void aListenedSliceOffersSelectTakeAndStop()
    {
        SliceChooser chooser;
        Row listened = row(0, Controller::OtherDevice, QStringLiteral("Jo's iPhone"));
        listened.listeningHere = true;
        listened.listenerCount = 2;
        chooser.setInventory({listened});
        QCOMPARE(actionWords(chooser),
                 (QStringList{QStringLiteral("Select RX"), QStringLiteral("Take control"),
                              QStringLiteral("Stop listening")}));
        QSignalSpy stop(&chooser, &SliceChooser::stopListeningRequested);
        QSignalSpy select(&chooser, &SliceChooser::selectRequested);
        action(chooser, QStringLiteral("Select RX"))->click();
        action(chooser, QStringLiteral("Stop listening"))->click();
        QCOMPARE(select.size(), 1);
        QCOMPARE(stop.size(), 1);
        render(chooser, QStringLiteral("Listening"), QStringLiteral("listened"));
    }

    // A slice this window controls: Release, nothing to take.
    void aControlledSliceOffersRelease()
    {
        SliceChooser chooser;
        Row mine = row(1, Controller::ThisWindow);
        mine.listeningHere = true;
        mine.activeHere = true;
        chooser.setInventory({mine});
        QCOMPARE(actionWords(chooser), QStringList{QStringLiteral("Release")});
        QSignalSpy release(&chooser, &SliceChooser::releaseRequested);
        action(chooser, QStringLiteral("Release"))->click();
        QCOMPARE(release.size(), 1);
        render(chooser, QStringLiteral("You control"), QStringLiteral("controlled"));
    }

    // On the air: Take control and Release wait until it stops.
    void aTransmittingSliceWaits()
    {
        SliceChooser chooser;
        Row onAir = row(0, Controller::OtherDevice, QStringLiteral("Jo's iPhone"));
        onAir.transmitting = true;
        Row mine = row(1, Controller::ThisWindow);
        mine.listeningHere = true;
        mine.activeHere = true;
        mine.transmitting = true;
        chooser.setInventory({onAir, mine});
        chooser.selectSlice(0);
        QVERIFY(!action(chooser, QStringLiteral("Take control"))->isEnabled());
        QVERIFY(action(chooser, QStringLiteral("Listen in"))->isEnabled());
        render(chooser, QStringLiteral("You control"), QStringLiteral("on-air"));
        chooser.selectSlice(1);
        QVERIFY(!action(chooser, QStringLiteral("Release"))->isEnabled());
    }

    // A request waits for the Core's answer, then shows it.
    void aRequestShowsWaitingThenTheAnswer()
    {
        SliceChooser chooser;
        chooser.setInventory({row(0, Controller::OtherDevice, QStringLiteral("Jo's iPhone"))});
        chooser.setPending(QStringLiteral("Asking the Core…"));
        QVERIFY(chooser.isPending());
        QVERIFY(!action(chooser, QStringLiteral("Listen in"))->isEnabled());
        QVERIFY(!chooser.findChild<QPushButton*>(QStringLiteral("sliceChooserNewSlice"))->isEnabled());
        QCOMPARE(chooser.message(), QStringLiteral("Asking the Core…"));
        render(chooser, QStringLiteral("Choose a slice"), QStringLiteral("waiting"));
        const QString refused =
            QStringLiteral("Slice A is transmitting. Take control once it stops.");
        chooser.showResult(refused);
        QVERIFY(!chooser.isPending());
        QCOMPARE(chooser.message(), refused);
        QVERIFY(action(chooser, QStringLiteral("Listen in"))->isEnabled());
    }

    // A request carries its verb; only the matching answer finishes it.
    void aRequestIsFinishedOnlyByItsOwnAnswer()
    {
        SliceChooser chooser;
        chooser.setInventory({row(0, Controller::OtherDevice, QStringLiteral("Jo's iPhone"))});
        chooser.beginRequest(QByteArrayLiteral("slice.listen"), QStringLiteral("Asking the Core…"),
                             QStringLiteral("Listening to slice A."));
        QCOMPARE(chooser.requestInFlight(), QByteArrayLiteral("slice.listen"));
        QVERIFY(chooser.isPending());
        QVERIFY(!chooser.finishRequest(QByteArrayLiteral("slice.release"), true, QString()));
        QVERIFY(chooser.isPending());
        QVERIFY(chooser.finishRequest(QByteArrayLiteral("slice.listen"), true, QString()));
        QVERIFY(!chooser.isPending());
        QVERIFY(chooser.requestInFlight().isEmpty());
        QCOMPARE(chooser.message(), QStringLiteral("Listening to slice A."));
    }

    // The link drops while a request waits: the chooser is not left stuck.
    void aDroppedLinkClearsTheWaitingRequest()
    {
        SliceChooser chooser;
        chooser.setInventory({row(0, Controller::OtherDevice, QStringLiteral("Jo's iPhone"))});
        chooser.beginRequest(QByteArrayLiteral("slice.takeControl"),
                             QStringLiteral("Asking the Core…"), QStringLiteral("You control it."));
        chooser.linkLost();
        QVERIFY(!chooser.isPending());
        QVERIFY(chooser.requestInFlight().isEmpty());
        QCOMPARE(chooser.message(), QStringLiteral("The Core did not answer"));
        QVERIFY(action(chooser, QStringLiteral("Listen in"))->isEnabled());
        QVERIFY(chooser.findChild<QPushButton*>(QStringLiteral("sliceChooserNewSlice"))->isEnabled());
        // A late answer for the dropped request changes nothing.
        QVERIFY(!chooser.finishRequest(QByteArrayLiteral("slice.takeControl"), true, QString()));
        QCOMPARE(chooser.message(), QStringLiteral("The Core did not answer"));
        // Nothing waiting: a dropped link leaves the last answer alone.
        chooser.showResult(QStringLiteral("You released slice A."));
        chooser.linkLost();
        QCOMPARE(chooser.message(), QStringLiteral("You released slice A."));
    }

    // Reopened while it shows waiting but no request is in flight.
    void reopeningClearsAWaitWithNoRequest()
    {
        SliceChooser chooser;
        chooser.setInventory({row(0, Controller::OtherDevice, QStringLiteral("Jo's iPhone"))});
        chooser.setPending(QStringLiteral("Asking the Core…"));
        chooser.reopened();
        QVERIFY(!chooser.isPending());
        QCOMPARE(chooser.message(), QStringLiteral("The Core did not answer"));
        // A request still in flight keeps waiting through a reopen.
        chooser.beginRequest(QByteArrayLiteral("slice.listen"), QStringLiteral("Asking the Core…"),
                             QStringLiteral("Listening to slice A."));
        chooser.reopened();
        QVERIFY(chooser.isPending());
        QCOMPARE(chooser.message(), QStringLiteral("Asking the Core…"));
    }

    // Task 14a: the slice flag's words come from the same row.
    void aFlagSaysWhoControlsTheSlice()
    {
        using State = VfoWidget::SliceAccess::State;

        Row mine = row(0, Controller::ThisWindow);
        mine.listeningHere = true;
        const VfoWidget::SliceAccess controlled = SliceChooser::flagAccessFor(mine);
        QCOMPARE(controlled.state, State::Controlled);
        QCOMPARE(controlled.line, QStringLiteral("You control"));
        QVERIFY(controlled.heldReason.isEmpty());

        Row theirs = row(1, Controller::OtherDevice, QStringLiteral("Jo's iPhone"));
        theirs.listeningHere = true;
        const VfoWidget::SliceAccess listened = SliceChooser::flagAccessFor(theirs);
        QCOMPARE(listened.state, State::Listening);
        QCOMPARE(listened.line, QStringLiteral("Listening \u00b7 controlled by Jo's iPhone"));
        QCOMPARE(listened.heldReason, QStringLiteral("Jo's iPhone controls this slice"));

        Row core = row(2, Controller::CoreDesktop);
        core.listeningHere = true;
        const VfoWidget::SliceAccess coreAccess = SliceChooser::flagAccessFor(core);
        QCOMPARE(coreAccess.state, State::Listening);
        QCOMPARE(coreAccess.line,
                 QStringLiteral("Listening \u00b7 controlled by the Core itself"));
        QCOMPARE(coreAccess.heldReason,
                 QStringLiteral("The Core itself controls this slice"));

        Row nobody = row(3, Controller::Nobody);
        nobody.listeningHere = true;
        const VfoWidget::SliceAccess unclaimed = SliceChooser::flagAccessFor(nobody);
        QCOMPARE(unclaimed.state, State::Listening);
        QCOMPARE(unclaimed.line, QStringLiteral("Listening \u00b7 nobody controls it"));
        QCOMPARE(unclaimed.heldReason,
                 QStringLiteral("Nobody controls this slice. Take control to change it."));

        // A slice this window neither controls nor listens to has no flag
        // access of its own.
        const Row other = row(1, Controller::OtherDevice, QStringLiteral("Jo's iPhone"));
        QCOMPARE(SliceChooser::flagAccessFor(other).state, State::Unshared);
    }

    // Task 14a: the flag waits for the same answer the chooser shows.
    void everyAnswerIsAnnounced()
    {
        SliceChooser chooser;
        chooser.setInventory({row(0, Controller::OtherDevice, QStringLiteral("Jo's iPhone"))});
        QSignalSpy shown(&chooser, &SliceChooser::resultShown);
        chooser.beginRequest(QByteArrayLiteral("slice.takeControl"),
                             QStringLiteral("Asking the Core\u2026"),
                             QStringLiteral("You control it."));
        QCOMPARE(shown.count(), 0);
        QVERIFY(chooser.finishRequest(QByteArrayLiteral("slice.takeControl"), true, QString()));
        QCOMPARE(shown.count(), 1);
        QCOMPARE(shown.first().first().toString(), QStringLiteral("You control it."));

        chooser.beginRequest(QByteArrayLiteral("slice.listen"),
                             QStringLiteral("Asking the Core\u2026"),
                             QStringLiteral("Listening to slice A."));
        chooser.linkLost();
        QCOMPARE(shown.count(), 2);
        QCOMPARE(shown.last().first().toString(), QStringLiteral("The Core did not answer"));
    }

    // No slice: honest words, and New slice.
    void anEmptyWindowOffersNewSlice()
    {
        SliceChooser chooser;
        chooser.setInventory({});
        QVERIFY(chooser.findChild<QLabel*>(QStringLiteral("sliceChooserEmpty")) != nullptr);
        QCOMPARE(chooser.findChild<QLabel*>(QStringLiteral("sliceChooserEmpty"))->text(),
                 QStringLiteral("No slices are running. Create a slice to start listening."));
        QVERIFY(actionWords(chooser).isEmpty());
        QSignalSpy newSlice(&chooser, &SliceChooser::newSliceRequested);
        chooser.findChild<QPushButton*>(QStringLiteral("sliceChooserNewSlice"))->click();
        QCOMPARE(newSlice.size(), 1);
        QCOMPARE(SliceChooser::bannerState({}), QStringLiteral("Choose a slice"));
        render(chooser, QStringLiteral("Choose a slice"), QStringLiteral("empty"));
        for (QLabel* l : chooser.findChildren<QLabel*>()) {
            QVERIFY2(OperatorWording::isPlain(l->text()) || l->text().isEmpty(),
                     qPrintable(l->text()));
        }
    }

    // A window on its own: every slice its own.
    void aLocalWindowListsItsOwnSlices()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);
        const int a = model.addSlice();
        model.sliceById(a)->setFrequency(7074000.0);
        const RemoteDevicesState none;
        const QList<Row> rows = SliceChooser::rowsForRemoteWindow(model, nullptr, none);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().controller, Controller::ThisWindow);
        QVERIFY(rows.first().listeningHere);
        SliceChooser chooser;
        chooser.setInventory(rows);
        render(chooser, SliceChooser::bannerState(rows), QStringLiteral("local"));
    }

    // A remote window: its own slice, a marker for another device's, and
    // the access objects saying who controls and listens.
    void aRemoteWindowListsEverySliceOnTheCore()
    {
        RadioModel model(RadioModel::Role::Remote);
        RemoteDevicesState devices;
        devices.setSelfDeviceId(QStringLiteral("me"));
        devices.applyObject("marker:0", {count("sliceId", 0), text("ownerDeviceId", "jo"),
                                         text("ownerName", "Jo's iPhone"),
                                         flag("ownerAway", true),
                                         MirrorUpdate{0, "frequency", MirrorWireKind::Float64,
                                                      QVariant(14074000.0)},
                                         count("dspMode", 1)});
        SliceAccessMirror access(nullptr, nullptr);
        access.setSelfDeviceId(QStringLiteral("me"));
        access.applyObject("access:0", {count("incarnation", 5), text("controllerDeviceId", "jo"),
                                        count("controlRevision", 2),
                                        text("listenerDeviceIds", "[\"jo\",\"me\"]"),
                                        text("activeRxDeviceIds", "[]"), flag("onAir", false)});
        const QList<Row> rows = SliceChooser::rowsForRemoteWindow(model, &access, devices);
        QCOMPARE(rows.size(), 1);
        const Row& r = rows.first();
        QCOMPARE(r.sliceId, 0);
        QCOMPARE(r.controller, Controller::OtherDevice);
        QCOMPARE(r.controllerName, QStringLiteral("Jo's iPhone"));
        QVERIFY(r.controllerAway);
        QVERIFY(r.listeningHere);
        QCOMPARE(r.listenerCount, 2);
        QCOMPARE(r.frequencyHz, 14074000.0);
        SliceChooser chooser;
        chooser.setInventory(rows);
        render(chooser, QStringLiteral("Listening"), QStringLiteral("remote-away"));
    }

    // Core-slice take-over (JJ, 2026-09-30): the Core's own slice's Take
    // control follows the Core's sliceAccessVersion. Below 3 the Core
    // refuses it, so it is disabled with the Core's words, never hidden;
    // at 3 it is offered like any other.
    void theCoresOwnSliceTakeFollowsTheCoresVersion()
    {
        const QString words = QStringLiteral("Slice A is run by the Core itself, so control of it cannot pass to this device.");
        RadioModel model(RadioModel::Role::Remote);
        RemoteDevicesState devices;
        devices.setSelfDeviceId(QStringLiteral("me"));
        // The Core's own slice: its marker names no device (kind station).
        devices.applyObject("marker:0", {count("sliceId", 0), text("ownerDeviceId", ""),
                                         text("ownerKind", "station"),
                                         MirrorUpdate{0, "frequency", MirrorWireKind::Float64,
                                                      QVariant(7177000.0)},
                                         count("dspMode", 1)});
        SliceAccessMirror access(nullptr, nullptr);
        access.setSelfDeviceId(QStringLiteral("me"));
        access.applyObject("access:0", {count("incarnation", 3),
                                        text("controllerDeviceId", "station"),
                                        count("controlRevision", 1),
                                        text("listenerDeviceIds", "[\"station\"]"),
                                        text("activeRxDeviceIds", "[]"), flag("onAir", false)});
        QVERIFY(!access.coreSliceTakeable());

        QList<Row> rows = SliceChooser::rowsForRemoteWindow(model, &access, devices);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().controller, Controller::CoreDesktop);
        QCOMPARE(rows.first().takeRefusal, words);
        SliceChooser chooser;
        chooser.setInventory(rows);
        chooser.selectSlice(0);
        QPushButton* take = action(chooser, QStringLiteral("Take control"));
        QVERIFY(take);
        QVERIFY(!take->isEnabled());
        QCOMPARE(take->toolTip(), words);
        QVERIFY(action(chooser, QStringLiteral("Listen in"))->isEnabled());
        render(chooser, QStringLiteral("Choose a slice"), QStringLiteral("core-slice-refused"));

        // Listening to it, the flag's Take control is off with the words.
        Row listened = rows.first();
        listened.listeningHere = true;
        QCOMPARE(SliceChooser::flagAccessFor(listened).takeHeldReason, words);

        QSignalSpy changed(&access, &SliceAccessMirror::changed);
        access.setCoreSliceTakeable(true);
        QCOMPARE(changed.count(), 1);
        rows = SliceChooser::rowsForRemoteWindow(model, &access, devices);
        QVERIFY(rows.first().takeRefusal.isEmpty());
        chooser.setInventory(rows);
        chooser.selectSlice(0);
        take = action(chooser, QStringLiteral("Take control"));
        QVERIFY(take && take->isEnabled());
        QSignalSpy taken(&chooser, &SliceChooser::takeControlRequested);
        take->click();
        QCOMPARE(taken.count(), 1);
        render(chooser, QStringLiteral("Choose a slice"), QStringLiteral("core-slice-offered"));
    }

    // Desktop listening lane (JJ, 2026-09-30): a slice the station device
    // holds names the desktop that hosts the Core, by its connectedDevices
    // entry with hostsCore. Its wire id is base64url("station"), so it is
    // never matched by the id "station".
    void aHostedCoreNamesTheHostingDesktop()
    {
        RadioModel model(RadioModel::Role::Remote);
        RemoteDevicesState devices;
        devices.setSelfDeviceId(QStringLiteral("me"));
        devices.applyObject("connectedDevices",
                            {text("listJson",
                                  QStringLiteral(
                                      "[{\"deviceId\":\"c3RhdGlvbg\",\"name\":\"Shack desktop\","
                                      "\"shortName\":\"Shack\",\"kind\":\"station\","
                                      "\"hostsCore\":true,\"state\":\"listening\"},"
                                      "{\"deviceId\":\"me\",\"name\":\"Jo's iPad\","
                                      "\"kind\":\"tablet\",\"state\":\"listening\"}]")),
                             count("deviceLimit", 4)});
        QVERIFY(!devices.connectedDevice(QStringLiteral("station")));
        QCOMPARE(devices.sliceHolderDevice(QStringLiteral("station"))->name,
                 QStringLiteral("Shack desktop"));
        devices.applyObject("marker:0", {count("sliceId", 0), text("ownerDeviceId", ""),
                                         text("ownerKind", "station"),
                                         MirrorUpdate{0, "frequency", MirrorWireKind::Float64,
                                                      QVariant(7177000.0)},
                                         count("dspMode", 1)});
        SliceAccessMirror access(nullptr, &devices);
        access.setSelfDeviceId(QStringLiteral("me"));
        access.setCoreSliceTakeable(true);
        access.applyObject("access:0", {count("incarnation", 3),
                                        text("controllerDeviceId", "station"),
                                        count("controlRevision", 1),
                                        text("listenerDeviceIds", "[\"station\",\"me\"]"),
                                        text("activeRxDeviceIds", "[]"), flag("onAir", false)});

        const QList<Row> rows = SliceChooser::rowsForRemoteWindow(model, &access, devices);
        QCOMPARE(rows.size(), 1);
        const Row& r = rows.first();
        QCOMPARE(r.controller, Controller::CoreDesktop);
        QCOMPARE(r.controllerName, QStringLiteral("Shack desktop"));
        QVERIFY(!r.controllerAway);
        QVERIFY(r.listeningHere);

        // The row, its description and the flag name the desktop.
        SliceChooser chooser;
        chooser.setInventory(rows);
        chooser.selectSlice(0);
        QStringList labels;
        for (QLabel* l : chooser.findChildren<QLabel*>()) {
            labels.append(l->text());
        }
        const QString all = labels.join(QLatin1Char('\n'));
        QVERIFY2(all.contains(QStringLiteral("Shack desktop controls")), qPrintable(all));
        QVERIFY2(all.contains(QStringLiteral("Shack desktop controls tuning.")), qPrintable(all));
        QVERIFY2(!all.contains(QStringLiteral("Core itself")), qPrintable(all));
        QVERIFY(action(chooser, QStringLiteral("Stop listening")));
        const VfoWidget::SliceAccess flagAccess = SliceChooser::flagAccessFor(r);
        QCOMPARE(flagAccess.line, QStringLiteral("Listening \u00b7 controlled by Shack desktop"));
        QCOMPARE(flagAccess.heldReason, QStringLiteral("Shack desktop controls this slice"));
        // The refusal a listened slice's held change shows, as the Core
        // words it (StationServer::sliceHolderWords).
        QCOMPARE(access.listenerReason(0),
                 QStringLiteral("Slice A is controlled by Shack desktop. Take control to change "
                                "it."));
        render(chooser, QStringLiteral("Listening"), QStringLiteral("hosted-core-named"));
    }

    // A Core no desktop hosts: the station device is the Core itself.
    void aHeadlessCoreIsTheCoreItself()
    {
        RadioModel model(RadioModel::Role::Remote);
        RemoteDevicesState devices;
        devices.setSelfDeviceId(QStringLiteral("me"));
        devices.applyObject("connectedDevices",
                            {text("listJson",
                                  QStringLiteral(
                                      "[{\"deviceId\":\"me\",\"name\":\"Jo's iPad\","
                                      "\"kind\":\"tablet\",\"state\":\"listening\"}]")),
                             count("deviceLimit", 4)});
        QVERIFY(!devices.sliceHolderDevice(QStringLiteral("station")));
        devices.applyObject("marker:0", {count("sliceId", 0), text("ownerDeviceId", ""),
                                         text("ownerKind", "station"),
                                         MirrorUpdate{0, "frequency", MirrorWireKind::Float64,
                                                      QVariant(7177000.0)},
                                         count("dspMode", 1)});
        SliceAccessMirror access(nullptr, &devices);
        access.setSelfDeviceId(QStringLiteral("me"));
        access.setCoreSliceTakeable(true);
        access.applyObject("access:0", {count("incarnation", 3),
                                        text("controllerDeviceId", "station"),
                                        count("controlRevision", 1),
                                        text("listenerDeviceIds", "[\"station\",\"me\"]"),
                                        text("activeRxDeviceIds", "[]"), flag("onAir", false)});

        const QList<Row> rows = SliceChooser::rowsForRemoteWindow(model, &access, devices);
        QCOMPARE(rows.size(), 1);
        const Row& r = rows.first();
        QCOMPARE(r.controller, Controller::CoreDesktop);
        QVERIFY(r.controllerName.isEmpty());
        SliceChooser chooser;
        chooser.setInventory(rows);
        chooser.selectSlice(0);
        QStringList labels;
        for (QLabel* l : chooser.findChildren<QLabel*>()) {
            labels.append(l->text());
        }
        const QString all = labels.join(QLatin1Char('\n'));
        QVERIFY2(all.contains(QStringLiteral("The Core itself controls")), qPrintable(all));
        QVERIFY2(all.contains(QStringLiteral("The Core itself controls tuning.")),
                 qPrintable(all));
        const VfoWidget::SliceAccess flagAccess = SliceChooser::flagAccessFor(r);
        QCOMPARE(flagAccess.line, QStringLiteral("Listening \u00b7 controlled by the Core itself"));
        QCOMPARE(flagAccess.heldReason, QStringLiteral("The Core itself controls this slice"));
        QCOMPARE(access.listenerReason(0),
                 QStringLiteral("Slice A is controlled by the Core. Take control to change it."));
    }

    // Job B review N-1 (JJ, 2026-09-30): the station marker names no
    // device (ownerDeviceId empty, ownerKind "station"), and the Core sends
    // the same for a slice nobody holds, so the marker alone cannot tell
    // them apart. The access entry decides: a controller of "station" is
    // the Core's desktop, named for the desktop that hosts the Core.
    void theStationMarkerIsTheCoresDesktop()
    {
        RadioModel model(RadioModel::Role::Remote);
        RemoteDevicesState devices;
        devices.setSelfDeviceId(QStringLiteral("me"));
        devices.applyObject("connectedDevices",
                            {text("listJson",
                                  QStringLiteral(
                                      "[{\"deviceId\":\"c3RhdGlvbg\",\"name\":\"Shack desktop\","
                                      "\"kind\":\"station\",\"hostsCore\":true,"
                                      "\"state\":\"listening\"}]")),
                             count("deviceLimit", 4)});
        devices.applyObject("marker:0", {count("sliceId", 0), text("ownerDeviceId", ""),
                                         text("ownerKind", "station"),
                                         MirrorUpdate{0, "frequency", MirrorWireKind::Float64,
                                                      QVariant(7177000.0)},
                                         count("dspMode", 1)});
        SliceAccessMirror access(nullptr, &devices);
        access.setSelfDeviceId(QStringLiteral("me"));
        access.setCoreSliceTakeable(true);
        access.applyObject("access:0", {count("incarnation", 3),
                                        text("controllerDeviceId", "station"),
                                        count("controlRevision", 1),
                                        text("listenerDeviceIds", "[\"station\"]"),
                                        text("activeRxDeviceIds", "[]"), flag("onAir", false)});
        const QList<Row> rows = SliceChooser::rowsForRemoteWindow(model, &access, devices);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().controller, Controller::CoreDesktop);
        QCOMPARE(rows.first().controllerName, QStringLiteral("Shack desktop"));
        QCOMPARE(rows.first().listenerCount, 1);
    }

    // Job B review N-1: the same marker for a slice nobody holds reads
    // "Available to control", whether the access entry names no controller
    // or there is no access data at all.
    void anUnownedMarkerIsAvailableToControl()
    {
        const auto ownerLine = [](const QList<Row>& rows) {
            SliceChooser chooser;
            chooser.setInventory(rows);
            QStringList labels;
            for (QLabel* l : chooser.findChildren<QLabel*>()) {
                labels.append(l->text());
            }
            return labels.join(QLatin1Char('\n'));
        };
        RadioModel model(RadioModel::Role::Remote);
        RemoteDevicesState devices;
        devices.setSelfDeviceId(QStringLiteral("me"));
        devices.applyObject("connectedDevices",
                            {text("listJson",
                                  QStringLiteral(
                                      "[{\"deviceId\":\"c3RhdGlvbg\",\"name\":\"Shack desktop\","
                                      "\"kind\":\"station\",\"hostsCore\":true,"
                                      "\"state\":\"listening\"}]")),
                             count("deviceLimit", 4)});
        devices.applyObject("marker:0", {count("sliceId", 0), text("ownerDeviceId", ""),
                                         text("ownerKind", "station"),
                                         MirrorUpdate{0, "frequency", MirrorWireKind::Float64,
                                                      QVariant(7177000.0)},
                                         count("dspMode", 1)});

        // An access entry whose controller is empty.
        SliceAccessMirror access(nullptr, &devices);
        access.setSelfDeviceId(QStringLiteral("me"));
        access.setCoreSliceTakeable(true);
        access.applyObject("access:0", {count("incarnation", 3),
                                        text("controllerDeviceId", ""),
                                        count("controlRevision", 2),
                                        text("listenerDeviceIds", "[]"),
                                        text("activeRxDeviceIds", "[]"), flag("onAir", false)});
        QList<Row> rows = SliceChooser::rowsForRemoteWindow(model, &access, devices);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().controller, Controller::Nobody);
        QVERIFY(rows.first().controllerName.isEmpty());
        QCOMPARE(rows.first().listenerCount, 0);
        QString all = ownerLine(rows);
        QVERIFY2(all.contains(QStringLiteral("Available to control")), qPrintable(all));
        QVERIFY2(!all.contains(QStringLiteral("Shack desktop")), qPrintable(all));

        // No access data at all.
        rows = SliceChooser::rowsForRemoteWindow(model, nullptr, devices);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().controller, Controller::Nobody);
        QCOMPARE(rows.first().listenerCount, 0);
        all = ownerLine(rows);
        QVERIFY2(all.contains(QStringLiteral("Available to control")), qPrintable(all));
        QVERIFY2(!all.contains(QStringLiteral("Shack desktop")), qPrintable(all));
    }

    // Job B review N-2 (JJ, 2026-09-30): a remote window's spectrum marker
    // for a slice the station device holds names the hosting desktop, as
    // the chooser does; a headless Core's reads the Core itself; nobody's
    // says nobody controls it. The click words offer Take control.
    void aStationMarkerOnTheSpectrumNamesTheHostingDesktop()
    {
        RemoteDevicesState devices;
        devices.setSelfDeviceId(QStringLiteral("me"));
        const auto hosted = [&devices](bool hosting) {
            devices.applyObject(
                "connectedDevices",
                {text("listJson",
                      hosting ? QStringLiteral(
                                    "[{\"deviceId\":\"c3RhdGlvbg\",\"name\":\"Shack desktop\","
                                    "\"shortName\":\"Shack\",\"kind\":\"station\","
                                    "\"hostsCore\":true,\"state\":\"listening\"}]")
                              : QStringLiteral("[]")),
                 count("deviceLimit", 4)});
        };
        devices.applyObject("marker:0", {count("sliceId", 0), text("ownerDeviceId", ""),
                                         text("ownerKind", "station"),
                                         MirrorUpdate{0, "frequency", MirrorWireKind::Float64,
                                                      QVariant(7177000.0)},
                                         count("dspMode", 1)});
        SliceAccessMirror access(nullptr, &devices);
        access.setSelfDeviceId(QStringLiteral("me"));
        const auto controlledBy = [&access](const QString& controller) {
            access.applyObject("access:0", {count("incarnation", 3),
                                            text("controllerDeviceId", controller),
                                            count("controlRevision", 1),
                                            text("listenerDeviceIds", "[]"),
                                            text("activeRxDeviceIds", "[]"),
                                            flag("onAir", false)});
        };

        // Hosted: the desktop's own name and short name.
        hosted(true);
        controlledBy(QStringLiteral("station"));
        QVector<SpectrumWidget::ForeignSliceMarker> markers =
            MultiDeviceController::foreignMarkers(devices, &access);
        QCOMPARE(markers.size(), 1);
        QCOMPARE(markers.first().ownerName, QStringLiteral("Shack desktop"));
        QCOMPARE(markers.first().ownerShortName, QStringLiteral("Shack"));
        QVERIFY(!markers.first().unowned);
        QCOMPARE(SpectrumWidget::foreignMarkerLabel(markers.first()), QStringLiteral("A Shack"));
        QCOMPARE(SpectrumWidget::foreignMarkerExplanation(markers.first()),
                 QStringLiteral("Slice A is controlled by Shack desktop. Take control to change "
                                "it."));

        // Headless: the Core itself.
        hosted(false);
        markers = MultiDeviceController::foreignMarkers(devices, &access);
        QCOMPARE(markers.first().ownerName, QStringLiteral("the Core itself"));
        QCOMPARE(SpectrumWidget::foreignMarkerExplanation(markers.first()),
                 QStringLiteral("Slice A is controlled by the Core itself. Take control to "
                                "change it."));

        // Nobody's, by the access entry and with no access data at all.
        hosted(true);
        controlledBy(QString());
        markers = MultiDeviceController::foreignMarkers(devices, &access);
        QVERIFY(markers.first().unowned);
        QVERIFY(markers.first().ownerName.isEmpty());
        QCOMPARE(SpectrumWidget::foreignMarkerLabel(markers.first()), QStringLiteral("A"));
        QCOMPARE(SpectrumWidget::foreignMarkerExplanation(markers.first()),
                 QStringLiteral("Nobody controls slice A. Take control to change it."));
        markers = MultiDeviceController::foreignMarkers(devices, nullptr);
        QVERIFY(markers.first().unowned);
        QVERIFY(markers.first().ownerName.isEmpty());
        const QString words = SpectrumWidget::foreignMarkerExplanation(markers.first());
        QVERIFY2(OperatorWording::isPlain(words), qPrintable(words));
    }

    // A slice the Core keeps for an away device: the row names that
    // device, never "the Core itself is away".
    void aSliceKeptForAnAwayDeviceNamesThatDevice()
    {
        RadioModel model(RadioModel::Role::Remote);
        RemoteDevicesState devices;
        devices.setSelfDeviceId(QStringLiteral("me"));
        devices.applyObject("marker:0", {count("sliceId", 0), text("ownerDeviceId", "jo"),
                                         text("ownerName", "Jo's iPhone"),
                                         text("ownerKind", "phone"), flag("ownerAway", true),
                                         MirrorUpdate{0, "frequency", MirrorWireKind::Float64,
                                                      QVariant(14074000.0)},
                                         count("dspMode", 1)});
        SliceAccessMirror access(nullptr, &devices);
        access.setSelfDeviceId(QStringLiteral("me"));
        // The Core holds it for Jo's iPhone: the station device controls it.
        access.applyObject("access:0", {count("incarnation", 5),
                                        text("controllerDeviceId", "station"),
                                        count("controlRevision", 3),
                                        text("listenerDeviceIds", "[\"station\"]"),
                                        text("activeRxDeviceIds", "[]"), flag("onAir", false)});
        const QList<Row> rows = SliceChooser::rowsForRemoteWindow(model, &access, devices);
        QCOMPARE(rows.size(), 1);
        const Row& r = rows.first();
        QCOMPARE(r.controller, Controller::OtherDevice);
        QCOMPARE(r.controllerName, QStringLiteral("Jo's iPhone"));
        QVERIFY(r.controllerAway);
        QVERIFY(r.takeRefusal.isEmpty());
        SliceChooser chooser;
        chooser.setInventory(rows);
        chooser.selectSlice(0);
        QStringList labels;
        for (QLabel* l : chooser.findChildren<QLabel*>()) {
            labels.append(l->text());
        }
        const QString all = labels.join(QLatin1Char('\n'));
        QVERIFY2(all.contains(QStringLiteral("Jo's iPhone is away. You can listen or take control "
                                             "now.")),
                 qPrintable(all));
        QVERIFY2(!all.contains(QStringLiteral("Core itself")), qPrintable(all));
        render(chooser, QStringLiteral("Choose a slice"), QStringLiteral("kept-for-away"));
    }

    // The hosting desktop: the Core's slices, two same-named devices told
    // apart by the names the Core numbers.
    void theHostingDesktopTellsSameNamedDevicesApart()
    {
        Core core;
        core.model->configureStreamPool(5, 5, 192000);
        Device first(QStringLiteral("MacBook-Pro"), QStringLiteral("computer"));
        Device second(QStringLiteral("MacBook-Pro"), QStringLiteral("computer"));
        core.pair(first);
        core.pair(second);
        LoopbackTransport* appFirst = core.signIn(first);
        LoopbackTransport* appSecond = core.signIn(second);
        QVERIFY(admitted(appFirst) && admitted(appSecond));
        const QList<Row> rows = SliceChooser::rowsForHostingDesktop(*core.model, *core.server);
        QStringList names;
        for (const Row& r : rows) {
            if (r.controller == Controller::OtherDevice) {
                names.append(r.controllerName);
            }
        }
        QCOMPARE(names.size(), 2);
        QVERIFY2(names.at(0) != names.at(1), qPrintable(names.join(QLatin1String(" | "))));
        SliceChooser chooser;
        chooser.setInventory(rows);
        render(chooser, SliceChooser::bannerState(rows), QStringLiteral("hosting-two-names"));
    }

    // Narrow: nothing is cut off; words wrap.
    void aNarrowChooserDoesNotClip()
    {
        SliceChooser chooser;
        chooser.setInventory({row(0, Controller::OtherDevice,
                                  QStringLiteral("A very long device name from the workshop")),
                              [] { Row r = row(1, Controller::ThisWindow);
                                   r.listeningHere = true; r.activeHere = true; return r; }()});
        chooser.selectSlice(0);
        chooser.resize(chooser.minimumSizeHint().width(), chooser.heightForWidth(300) > 0
                                                             ? chooser.heightForWidth(300)
                                                             : chooser.sizeHint().height());
        chooser.resize(300, chooser.sizeHint().height());
        QCoreApplication::processEvents();
        for (QPushButton* b : chooser.findChildren<QPushButton*>(QStringLiteral("sliceChooserAction"))) {
            QVERIFY2(b->sizeHint().width() <= chooser.width(), qPrintable(b->text()));
        }
        QVERIFY(chooser.minimumSizeHint().width() <= 320);
        render(chooser, QStringLiteral("You control"), QStringLiteral("narrow"));
    }
};

QTEST_MAIN(TstSliceChooser)
#include "tst_slice_chooser.moc"
