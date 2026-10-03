// =================================================================
// tests/tst_remote_window_slice_a_close.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Drives one real remote MainWindow
// against an in-process Core; no upstream logic is ported.
//
// VFO flag crash lane (2026-09-30). JJ's remote window crashed in
// SliceModel::lockedChanged after Slice A was closed from the X on its
// flag: Slice A's flag outlived its slice, its buttons still wrote the
// freed SliceModel, and a Slice A the Core made again was never bound to
// a flag, so it did not paint. Every step starts from the window's own
// controls or from the Core's side of the wire.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30  J.J. Boyd / KG4VCF  VFO flag crash lane: close Slice A
//                                    from its flag, click what remains,
//                                    make Slice A again.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-01  J.J. Boyd / KG4VCF  VFO flag crash lane fix round: Slice A
//                                    made again from its own empty pan
//                                    while the other pan is active; its
//                                    flag sits on the pan its pan key
//                                    names. AI-assisted via Anthropic
//                                    Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QAction>
#include <QLoggingCategory>
#include <QMenu>
#include <QPointer>
#include <QPushButton>

#include "core/SliceOwnership.h"
#include "core/session/DeviceSessionRegistry.h"
#include "core/session/SliceAccessMirror.h"
#include "core/session/StationClient.h"
#include "fakes/RemoteWindowHarness.h"
#include "gui/MainWindow.h"
#include "gui/PanadapterApplet.h"
#include "gui/PanadapterStack.h"
#include "gui/SpectrumWidget.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;
using NereusSDR::Test::RemoteWindowHarness;

namespace {

const QByteArray kSelf = QByteArrayLiteral("token:1");

void clickLeft(QWidget* widget)
{
    QTest::mouseClick(widget, Qt::LeftButton, Qt::NoModifier, widget->rect().center());
}

VfoWidget* flagFor(RemoteWindowHarness& h, int sliceId)
{
    for (VfoWidget* flag : h.window()->findChildren<VfoWidget*>()) {
        if (flag->sliceIndex() == sliceId) { return flag; }
    }
    return nullptr;
}

int flagsFor(RemoteWindowHarness& h, int sliceId)
{
    int count = 0;
    for (VfoWidget* flag : h.window()->findChildren<VfoWidget*>()) {
        if (flag->sliceIndex() == sliceId) { ++count; }
    }
    return count;
}

// Starts the window's own connection and waits until it shares slices and
// controls each slice the Core has.
bool connectSharing(RemoteWindowHarness& h)
{
    h.startStartupConnection();
    StationClient* client = h.client();
    if (!client || !QTest::qWaitFor([client] { return client->stationLinkReady(); }, 10000)) {
        return false;
    }
    if (!client->remoteSliceAccessAvailable()) { return false; }
    return QTest::qWaitFor([&h, client] {
        const QList<int> live = h.station().sliceOwnership()->liveSlices();
        for (int id : live) {
            const auto entry = client->sliceAccess()->entry(id);
            if (!entry || entry->controllerDeviceId != QString::fromLatin1(kSelf)) { return false; }
        }
        return !live.isEmpty();
    }, 10000);
}

QByteArray admitPhone(RemoteWindowHarness& h, QObject& session)
{
    DeviceSessionRegistry::Entry phone;
    phone.deviceId = QByteArrayLiteral("phone-device-id-for-slice-a-close1");
    phone.kind = DeviceSessionRegistry::Kind::Paired;
    phone.name = QStringLiteral("Living room iPhone");
    phone.shortName = QStringLiteral("iPhone");
    phone.deviceKind = QStringLiteral("phone");
    const bool admitted = h.server().deviceSessions()->admit(phone, &session).admission
        == DeviceSessionRegistry::Admission::Admitted;
    return admitted ? phone.deviceId : QByteArray();
}

// Clicks the flag's floating lock button twice (on, then off again), then
// every checkable button on the flag itself twice, so each flag control
// that writes its slice runs and the flag is left as it was. The TX badge
// is left alone: it asks the Core for transmit.
void clickFlagButtons(VfoWidget* flag)
{
    QPushButton* lock = flag->lockButtonForTest();
    QVERIFY(lock);
    clickLeft(lock);
    clickLeft(lock);
    for (QAbstractButton* button : flag->findChildren<QAbstractButton*>()) {
        if (!button->isCheckable() || !button->isEnabled()
            || button->objectName() == QStringLiteral("VfoTxBadge")) {
            continue;
        }
        button->click();
        button->click();
    }
}

PanadapterApplet* panApplet(RemoteWindowHarness& h, const QString& panId)
{
    for (PanadapterApplet* applet : h.window()->findChildren<PanadapterApplet*>()) {
        if (applet->panId() == panId) { return applet; }
    }
    return nullptr;
}

// Connects with two slices on a two-pan layout, has this window be the only
// one on Slice A (a phone took it and let go), and closes Slice A from the X
// on its flag. Returns with the Core and the window both without Slice A,
// and every flag left in the window clicked. `sliceAPan` is the pan Slice A
// was on.
void closeSliceAFromItsFlag(RemoteWindowHarness& h, QString& sliceAPan)
{
    QVERIFY(h.start());
    QVERIFY(connectSharing(h));
    RadioModel* remote = h.remoteModel();
    QVERIFY(remote);
    QTRY_VERIFY(remote->sliceById(0) && remote->sliceById(1));
    QTRY_VERIFY(flagFor(h, 0) && flagFor(h, 1));
    QTRY_VERIFY(!remote->sliceById(0)->panKey().isEmpty());
    sliceAPan = remote->sliceById(0)->panKey();

    // A phone takes Slice A, so this window listens to it; then the
    // phone lets go and only this window is left on it.
    QObject phoneSession;
    const QByteArray phone = admitPhone(h, phoneSession);
    QVERIFY(!phone.isEmpty());
    SliceOwnership* ownership = h.station().sliceOwnership();
    ownership->setOwner(0, phone);
    QTRY_VERIFY(flagFor(h, 0)->isListening());
    ownership->setOwner(0, QByteArray());
    ownership->leave(phone, 0);
    QVERIFY(ownership->listenersOf(0) == QList<QByteArray>({kSelf}));
    QVERIFY(ownership->isListening(kSelf, 0));
    QTRY_VERIFY(flagFor(h, 0)->isListening());

    const QPointer<SliceModel> oldSliceA(remote->sliceById(0));
    VfoWidget* oldFlagA = flagFor(h, 0);
    QPushButton* close = oldFlagA->closeButtonForTest();
    QVERIFY(close);
    QTRY_VERIFY(close->isVisible());
    clickLeft(close);
    QTRY_VERIFY2(h.sliceAccessCommands().contains(QStringLiteral("slice.stopListening:0")),
                 qPrintable(h.sliceAccessCommands().join(QLatin1Char(','))));
    QTRY_VERIFY(!ownership->isListening(kSelf, 0));
    QVERIFY2(ownership->mark(0).owner.isEmpty(), ownership->mark(0).owner.constData());

    // The Core closes it, and the window's slice is freed.
    QTRY_VERIFY(h.station().sliceById(0) == nullptr);
    QTRY_VERIFY(remote->sliceById(0) == nullptr);
    QTRY_VERIFY(oldSliceA.isNull());

    // Every flag still in the window: its lock and its other buttons
    // write only a live slice (the crash was a click here).
    for (VfoWidget* flag : h.window()->findChildren<VfoWidget*>()) {
        clickFlagButtons(flag);
    }
    QCoreApplication::processEvents();

    // No flag is left for the closed slice.
    QCOMPARE(flagsFor(h, 0), 0);
    QVERIFY(flagFor(h, 1));
    QCOMPARE(flagFor(h, 1)->sliceForTest(), remote->sliceById(1));
}

// The Core tunes the new Slice A: its flag and the pan its pan key names
// follow, and the flag's lock writes the new slice.
void newSliceAPaintsOnItsPan(RemoteWindowHarness& h)
{
    RadioModel* remote = h.remoteModel();
    QTRY_VERIFY(h.station().sliceById(0) != nullptr);
    QTRY_VERIFY(remote->sliceById(0) != nullptr);
    SliceModel* newSliceA = remote->sliceById(0);
    QTRY_VERIFY(flagFor(h, 0) != nullptr);
    QCOMPARE(flagsFor(h, 0), 1);
    VfoWidget* newFlagA = flagFor(h, 0);
    QCOMPARE(newFlagA->sliceForTest(), newSliceA);

    // The flag sits on the pan its slice's pan key names.
    QTRY_VERIFY(!newSliceA->panKey().isEmpty());
    SpectrumWidget* host = h.panSpectrum(newSliceA->panKey());
    QVERIFY(host);
    QTRY_VERIFY(flagFor(h, 0) && flagFor(h, 0)->parentWidget() == host);
    QCOMPARE(flagsFor(h, 0), 1);
    newFlagA = flagFor(h, 0);
    QCOMPARE(newFlagA->sliceForTest(), newSliceA);

    SliceModel* coreSliceA = h.station().sliceById(0);
    QVERIFY(coreSliceA);
    const double tuned = coreSliceA->frequency() + 1500.0;
    coreSliceA->setFrequency(tuned);
    QTRY_COMPARE(newSliceA->frequency(), tuned);
    QTRY_COMPARE(newFlagA->frequency(), tuned);
    QTRY_COMPARE(host->vfoFrequencyForTest(), tuned);
    coreSliceA->setFilter(-2900, -150);
    QTRY_COMPARE(newSliceA->filterLow(), -2900);
    QTRY_COMPARE(host->filterLowForTest(), -2900);
    QCOMPARE(host->filterHighForTest(), -150);

    QVERIFY(!newSliceA->locked());
    clickLeft(newFlagA->lockButtonForTest());
    QVERIFY(newSliceA->locked());
    clickLeft(newFlagA->lockButtonForTest());
    QVERIFY(!newSliceA->locked());
}

RemoteWindowHarness::Options twoSlicesTwoPans()
{
    RemoteWindowHarness::Options options;
    options.stationSlices = 2;
    options.panLayout = QStringLiteral("2v");
    options.sliceAccess = true;
    return options;
}

} // namespace

class TestRemoteWindowSliceAClose final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
        QVERIFY(RemoteWindowHarness::useIsolatedProfile(QStringLiteral("remote-window-slice-a-close")));
    }

    void init()
    {
        QVERIFY(RemoteWindowHarness::clearIsolatedProfile());
    }

    void cleanupTestCase()
    {
        QVERIFY(RemoteWindowHarness::removeIsolatedProfile());
    }

    // JJ's crash: Slice A closed from the X on its flag. The window listens
    // to Slice A after the phone that controlled it let go, so the X is the
    // flag's Stop listening; with nobody left on it the Core closes the
    // slice. Every control on every flag that remains is then clicked, and
    // Slice A is made again from the window's menu: its flag and its pan's
    // marker and passband follow the new slice.
    void closingSliceAFromItsFlagLeavesNoFlagAndANewSliceAPaints()
    {
        RemoteWindowHarness h(twoSlicesTwoPans());
        QString sliceAPan;
        closeSliceAFromItsFlag(h, sliceAPan);
        if (QTest::currentTestFailed()) { return; }

        // Slice A made again, from the window's own menu.
        QAction* add = h.menuAction(QStringLiteral("&View"), QStringLiteral("&Add slice on active pan"));
        QVERIFY(add);
        QTRY_VERIFY(add->isEnabled());
        add->trigger();
        newSliceAPaintsOnItsPan(h);
    }

    // The fix round's case: Slice A closed, the other pan made active, and
    // Slice A made again from its own empty pan. The window is sent the
    // slice before its pan key, so it is docked on the active pan first;
    // its flag has to move to the pan the key names.
    void sliceAMadeAgainOnItsOwnPanWhileTheOtherIsActiveGetsItsFlagThere()
    {
        RemoteWindowHarness h(twoSlicesTwoPans());
        QString sliceAPan;
        closeSliceAFromItsFlag(h, sliceAPan);
        if (QTest::currentTestFailed()) { return; }

        RadioModel* remote = h.remoteModel();
        const QString otherPan = remote->sliceById(1)->panKey();
        QVERIFY2(!sliceAPan.isEmpty() && otherPan != sliceAPan,
                 qPrintable(sliceAPan + QLatin1Char('/') + otherPan));

        // The other pan becomes active the way the operator makes it so: a
        // click on it.
        auto* stack = h.window()->findChild<PanadapterStack*>();
        QVERIFY(stack);
        PanadapterApplet* other = panApplet(h, otherPan);
        QVERIFY(other);
        QTest::mouseClick(other, Qt::LeftButton);
        QTRY_COMPARE(stack->activePanId(), otherPan);

        // Slice A's old pan, now empty, asks for a slice on itself.
        PanadapterApplet* own = panApplet(h, sliceAPan);
        QVERIFY(own);
        QMenu* menu = own->buildContextMenuForTesting();
        QVERIFY(menu);
        QAction* addHere = nullptr;
        for (QAction* action : menu->actions()) {
            if (action->text() == QStringLiteral("Add slice on this pan")) { addHere = action; }
        }
        QVERIFY(addHere);
        addHere->trigger();
        QTRY_VERIFY2(h.addSliceCommands().join(QLatin1Char(',')).contains(sliceAPan),
                     qPrintable(h.addSliceCommands().join(QLatin1Char(','))));
        QTRY_VERIFY(remote->sliceById(0) != nullptr);
        QTRY_COMPARE(remote->sliceById(0)->panKey(), sliceAPan);
        newSliceAPaintsOnItsPan(h);
    }
};

QTEST_MAIN(TestRemoteWindowSliceAClose)
#include "tst_remote_window_slice_a_close.moc"
