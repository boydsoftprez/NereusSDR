// SPDX-License-Identifier: GPL-3.0-or-later
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 3.
//                AI-assisted implementation via Anthropic Claude Code.
// no-port-check: NereusSDR-original test.
// Hot keys on one real remote MainWindow against an in-process Core: an action
// reaches the Core by the route its on-screen control uses, and a listened
// slice refuses as the window's other controls refuse it.
#include <QtTest/QtTest>

#include <QLoggingCategory>

#include "core/SliceOwnership.h"
#include "core/session/DeviceSessionRegistry.h"
#include "core/session/SliceAccessController.h"
#include "core/session/SliceAccessMirror.h"
#include "core/session/StationClient.h"
#include "fakes/RemoteWindowHarness.h"
#include "gui/MainWindow.h"
#include "gui/SpectrumWidget.h"
#include "gui/hotkeys/HotKeyDispatcher.h"
#include "gui/hotkeys/MainWindowHotKeyTargets.h"
#include "gui/widgets/StatusToast.h"
#include "gui/widgets/VfoWidget.h"
#include "models/BandDefaults.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;
using NereusSDR::Test::RemoteWindowHarness;

namespace {

const QByteArray kSelf = QByteArrayLiteral("token:1");
const HotKeySource kKeyboard{HotKeySourceKind::Keyboard, QString()};

VfoWidget* flagFor(RemoteWindowHarness& h, int sliceId)
{
    for (VfoWidget* flag : h.window()->findChildren<VfoWidget*>()) {
        if (flag->sliceIndex() == sliceId) {
            return flag;
        }
    }
    return nullptr;
}

bool toastShown(RemoteWindowHarness& h, const QString& message)
{
    for (StatusToast* toast : h.window()->findChildren<StatusToast*>()) {
        if (toast->message() == message) {
            return true;
        }
    }
    return false;
}

// Starts the window's own connection and waits until it shares slices and
// controls each slice the Core has (tst_remote_window_slice_a_close).
bool connectSharing(RemoteWindowHarness& h)
{
    h.startStartupConnection();
    StationClient* const client = h.client();
    if (client == nullptr
        || !QTest::qWaitFor([client] { return client->stationLinkReady(); }, 10000)) {
        return false;
    }
    if (!client->remoteSliceAccessAvailable()) {
        return false;
    }
    return QTest::qWaitFor([&h, client] {
        const QList<int> live = h.station().sliceOwnership()->liveSlices();
        for (const int id : live) {
            const auto entry = client->sliceAccess()->entry(id);
            if (!entry || entry->controllerDeviceId != QString::fromLatin1(kSelf)) {
                return false;
            }
        }
        return !live.isEmpty();
    }, 10000);
}

QByteArray admitPhone(RemoteWindowHarness& h, QObject& session)
{
    DeviceSessionRegistry::Entry phone;
    phone.deviceId = QByteArrayLiteral("phone-device-id-for-hot-keys-0001");
    phone.kind = DeviceSessionRegistry::Kind::Paired;
    phone.name = QStringLiteral("Living room iPhone");
    phone.shortName = QStringLiteral("iPhone");
    phone.deviceKind = QStringLiteral("phone");
    const bool admitted = h.server().deviceSessions()->admit(phone, &session).admission
        == DeviceSessionRegistry::Admission::Admitted;
    return admitted ? phone.deviceId : QByteArray();
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

class TestHotKeyRemoteActions final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
        QVERIFY(RemoteWindowHarness::useIsolatedProfile(QStringLiteral("hot-key-remote-actions")));
    }

    void init()
    {
        QVERIFY(RemoteWindowHarness::clearIsolatedProfile());
    }

    void cleanupTestCase()
    {
        QVERIFY(RemoteWindowHarness::removeIsolatedProfile());
    }

    void tuneReachesTheCoreThroughTheFlag()
    {
        RemoteWindowHarness h(twoSlicesTwoPans());
        QVERIFY(h.start());
        QVERIFY(connectSharing(h));
        RadioModel* const remote = h.remoteModel();
        QTRY_VERIFY(remote->sliceById(0) != nullptr);
        QTRY_VERIFY(flagFor(h, 0) != nullptr);
        SliceModel* const slice = remote->sliceById(0);
        SliceModel* const core = h.station().sliceById(0);
        QVERIFY(core != nullptr);

        core->setFrequency(14'200'000.0);
        QTRY_COMPARE(slice->frequency(), 14'200'000.0);
        QTRY_COMPARE(flagFor(h, 0)->frequency(), 14'200'000.0);
        slice->setStepHz(100);

        h.window()->hotKeyDispatcher()->press(QStringLiteral("tune.up"), kKeyboard, QLatin1Char('A'));

        QCOMPARE(flagFor(h, 0)->frequencyLabelForTest(), QStringLiteral("14.200.100"));
        QTRY_COMPARE(core->frequency(), 14'200'100.0);
    }

    void aListenedSliceRefusesTuneAndVolumeAsksForThisDevicesLevel()
    {
        RemoteWindowHarness h(twoSlicesTwoPans());
        QVERIFY(h.start());
        QVERIFY(connectSharing(h));
        RadioModel* const remote = h.remoteModel();
        QTRY_VERIFY(remote->sliceById(0) != nullptr);
        QTRY_VERIFY(flagFor(h, 0) != nullptr);

        // A phone takes Slice A, so this window listens to it.
        QObject phoneSession;
        const QByteArray phone = admitPhone(h, phoneSession);
        QVERIFY(!phone.isEmpty());
        SliceOwnership* const ownership = h.station().sliceOwnership();
        ownership->setOwner(0, phone);
        QTRY_VERIFY(flagFor(h, 0)->isListening());

        SliceModel* const slice = remote->sliceById(0);
        VfoWidget* const flag = flagFor(h, 0);
        HotKeyDispatcher* const keys = h.window()->hotKeyDispatcher();
        MainWindowHotKeyTargets targets(h.window());
        QVERIFY(targets.listened(slice));
        const QString refusal = targets.sliceRefusal(slice);
        QVERIFY(refusal.contains(QStringLiteral("iPhone")));

        const double before = slice->frequency();
        keys->press(QStringLiteral("tune.up"), kKeyboard, QLatin1Char('A'));
        QCOMPARE(slice->frequency(), before);
        QVERIFY2(toastShown(h, refusal), qPrintable(refusal));

        // Filter, mode and band refuse with the same words.
        const QStringList refused = {QStringLiteral("filter.next"), QStringLiteral("mode.next"),
                                     QStringLiteral("band.next"), QStringLiteral("nr.toggle")};
        QSignalSpy mode(slice, &SliceModel::dspModeChanged);
        QSignalSpy filter(slice, &SliceModel::filterChanged);
        for (const QString& id : refused) {
            qDeleteAll(h.window()->findChildren<StatusToast*>());
            keys->press(id, kKeyboard, QLatin1Char('A'));
            QVERIFY2(toastShown(h, refusal), qPrintable(id));
        }
        QCOMPARE(mode.count(), 0);
        QCOMPARE(filter.count(), 0);

        // Volume asks the Core for this device's level and paints the flag.
        SliceAccessController* const access = h.server().sliceAccessController();
        QVERIFY(access != nullptr);
        QSignalSpy level(access, &SliceAccessController::listenLevelChanged);
        QSignalSpy af(slice, &SliceModel::afGainChanged);
        const int start = flag->listenVolume();
        QVERIFY(start >= 5);
        keys->press(QStringLiteral("volume.down"), kKeyboard, QLatin1Char('A'));
        QCOMPARE(flag->listenVolume(), start - 5);
        QTRY_VERIFY(level.count() >= 1);
        QTRY_COMPARE(qRound(access->listenLevel(kSelf, 0).level * 100.0), start - 5);

        const bool muted = flag->listenMuted();
        keys->press(QStringLiteral("mute"), kKeyboard, QLatin1Char('A'));
        QCOMPARE(flag->listenMuted(), !muted);
        QTRY_COMPARE(access->listenLevel(kSelf, 0).muted, !muted);
        QCOMPARE(af.count(), 0);
    }

    // R-HK-07, the remote half; tst_hot_key_window_actions has the local one.
    void theSelectedFlagChoosesBothSliceAndPan()
    {
        RemoteWindowHarness h(twoSlicesTwoPans());
        QVERIFY(h.start());
        QVERIFY(connectSharing(h));
        RadioModel* const remote = h.remoteModel();
        QTRY_VERIFY(remote->sliceById(0) != nullptr && remote->sliceById(1) != nullptr);
        QTRY_VERIFY(flagFor(h, 0) != nullptr && flagFor(h, 1) != nullptr);

        MainWindowHotKeyTargets targets(h.window());
        for (const int id : {1, 0}) {
            SliceModel* const slice = remote->sliceById(id);
            flagFor(h, id)->requestActivation();
            QTRY_COMPARE(targets.windowSlice(), slice);
            QTRY_COMPARE(targets.zoomTarget(nullptr), h.window()->spectrumForSlice(slice));
        }
    }

    void bandWalkSkips2mOnACoreWithoutIt()
    {
        RemoteWindowHarness h(twoSlicesTwoPans());
        QVERIFY(h.start());
        StationClient* const client = h.client();
        QVERIFY(client != nullptr);
        client->withholdFeatureForTest(QByteArrayLiteral("band2m"));
        QVERIFY(connectSharing(h));
        QVERIFY(!client->band2mUnavailableReason().isEmpty());

        RadioModel* const remote = h.remoteModel();
        QTRY_VERIFY(remote->sliceById(0) != nullptr);
        SliceModel* const slice = remote->sliceById(0);
        SliceModel* const core = h.station().sliceById(0);
        QVERIFY(core != nullptr);

        core->setFrequency(50'100'000.0);
        QTRY_COMPARE(slice->frequency(), 50'100'000.0);

        h.window()->hotKeyDispatcher()->press(QStringLiteral("band.next"), kKeyboard, QLatin1Char('A'));
        QTRY_COMPARE(BandDefaults::bandGridBandFromFrequency(slice->frequency()), Band::WWV);
    }
};

QTEST_MAIN(TestHotKeyRemoteActions)
#include "tst_hot_key_remote_actions.moc"
