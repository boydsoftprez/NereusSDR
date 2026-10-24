// no-port-check: NereusSDR-original test.
//
// tests/tst_limit_filters_to_sidebands.cpp
//
// Setup > General > Filters' "Limit filter edges to sidebands" (Thetis
// chkLimitFilterEdgesToSidebands, setup.designer.cs:12426-12437 and
// setup.cs:22378-22381 [v2.10.3.15]), which sets the flag ConstrainFilter
// reads (console.cs:7516-7521, 35044-35131 [v2.10.3.15]).
//
// Covered: off by default; on, a slice's filter edge stops at the carrier
// and off it does not; the box saves "True"/"False" in AppSettings and a new
// model reads it; a change reaches every slice at once, and a slice made
// later; the key is the Core's (Station scope); a remote window's box
// reaches the Core's settings and the Core's slices over the in-process
// loopback, and the Core's change reaches the window; removing the key on
// the Core applies the default; an older Core's refusal puts the box back
// and says so. No hardware; no audio device is opened by these objects.
//
// Modification history (NereusSDR):
//   2026-10-24: created by J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.

#include <QtTest/QtTest>

#include <QCheckBox>
#include <QLabel>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/WdspTypes.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "core/settings/SettingsScope.h"
#include "gui/setup/GeneralFiltersPage.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/LoopbackTransport.h"
#include "OperatorWording.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kKey = QStringLiteral("LimitFiltersToSidebands");

class ScopedRemoteBackend final {
public:
    explicit ScopedRemoteBackend(ISettingsBackend* backend)
    {
        AppSettings::instance().setRemoteBackend(backend);
    }
    ~ScopedRemoteBackend() { AppSettings::instance().setRemoteBackend(nullptr); }
};

// An LSB filter whose upper edge is past the carrier: the limit stops it
// at 0 Hz (console.cs:35055-35059 [v2.10.3.15]).
void requestLsbAcrossTheCarrier(SliceModel& slice)
{
    slice.setDspMode(DSPMode::LSB);
    slice.setFilter(-2850, 500);
}

} // namespace

class TestLimitFiltersToSidebands final : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_securityDir;

private slots:
    void initTestCase() { QVERIFY(m_securityDir.isValid()); }
    void init() { AppSettings::instance().remove(kKey); }
    void cleanup() { AppSettings::instance().remove(kKey); }

    void offByDefault()
    {
        QCOMPARE(RadioModel::kLimitFiltersToSidebandsDefault, false);
        QVERIFY(!RadioModel::limitFiltersToSidebandsSetting());
        RadioModel model;
        QVERIFY(!model.limitFiltersToSidebands());
        model.addSlice();
        QVERIFY(!model.slices().isEmpty());
        SliceModel* slice = model.slices().first();
        QVERIFY(!slice->limitFiltersToSidebands());
        requestLsbAcrossTheCarrier(*slice);
        QCOMPARE(slice->filterHigh(), 500);

        GeneralFiltersPage page(&model);
        QCheckBox* box = page.limitFilterEdgesCheckBox();
        QVERIFY(box != nullptr);
        QCOMPARE(box->objectName(), QStringLiteral("chkLimitFilterEdgesToSidebands"));
        QCOMPARE(box->text(), QStringLiteral("Limit filter edges to sidebands"));
        QCOMPARE(box->toolTip(),
                 QStringLiteral("Prevents crossing the 0 point with certain filters"));
        QVERIFY(!box->isChecked());
        // Building the page writes nothing.
        QVERIFY(!AppSettings::instance().contains(kKey));
    }

    void onClampsAtTheCarrierAndOffDoesNot()
    {
        RadioModel model;
        model.addSlice();
        SliceModel* slice = model.slices().first();
        GeneralFiltersPage page(&model);
        QCheckBox* box = page.limitFilterEdgesCheckBox();

        box->setChecked(true);
        requestLsbAcrossTheCarrier(*slice);
        QCOMPARE(slice->filterLow(), -2850);
        QCOMPARE(slice->filterHigh(), 0);

        // Off again: the next request is not limited. As in Thetis, turning
        // it off does not re-apply the stored filter; the next change is
        // what it governs.
        box->setChecked(false);
        QCOMPARE(slice->filterHigh(), 0);
        slice->setFilter(-2850, 500);
        QCOMPARE(slice->filterHigh(), 500);
    }

    void boxIsSavedAndReadBack()
    {
        {
            RadioModel model;
            GeneralFiltersPage page(&model);
            page.limitFilterEdgesCheckBox()->setChecked(true);
            QCOMPARE(AppSettings::instance().value(kKey).toString(), QStringLiteral("True"));
        }
        {
            RadioModel model;
            QVERIFY(model.limitFiltersToSidebands());
            GeneralFiltersPage page(&model);
            QVERIFY(page.limitFilterEdgesCheckBox()->isChecked());
            page.limitFilterEdgesCheckBox()->setChecked(false);
            QCOMPARE(AppSettings::instance().value(kKey).toString(), QStringLiteral("False"));
        }
        RadioModel model;
        QVERIFY(!model.limitFiltersToSidebands());
    }

    void aChangeReachesEverySliceAtOnce()
    {
        RadioModel model;
        model.addSlice();
        model.addSlice();
        QCOMPARE(model.slices().size(), 2);
        GeneralFiltersPage page(&model);
        QSignalSpy changed(&model, &RadioModel::limitFiltersToSidebandsChanged);

        page.limitFilterEdgesCheckBox()->setChecked(true);
        QCOMPARE(changed.size(), 1);
        for (SliceModel* slice : model.slices()) {
            QVERIFY(slice->limitFiltersToSidebands());
        }
        // A slice made afterwards takes it before its filter is restored.
        model.addSlice();
        QCOMPARE(model.slices().size(), 3);
        QVERIFY(model.slices().last()->limitFiltersToSidebands());

        // The box follows the model when the change comes from elsewhere.
        model.applyLimitFiltersToSidebands(false);
        QVERIFY(!page.limitFilterEdgesCheckBox()->isChecked());
        for (SliceModel* slice : model.slices()) {
            QVERIFY(!slice->limitFiltersToSidebands());
        }
    }

    void theKeyIsTheCores()
    {
        QCOMPARE(classifySettingsKey(kKey), SettingsScope::Station);
    }

    void remoteBoxReachesTheCoreAndTheCoreReachesTheWindow()
    {
        QTemporaryDir dir;
        AppSettings coreSettings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel core;
        core.addSlice();
        SliceModel* coreSlice = core.slices().first();
        StationServer server(&core, coreSettings,
                             NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
        RadioModel window(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&window, &proxy);
        auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
        stationEnd->linkTo(clientEnd);
        QSignalSpy completed(&client, &StationClient::handshakeComplete);
        client.startSession(clientEnd, server.token());
        server.acceptTransport(stationEnd);
        QVERIFY(completed.wait(5000) || !completed.isEmpty());

        ScopedRemoteBackend backend(&proxy);
        GeneralFiltersPage page(&window);
        QCheckBox* box = page.limitFilterEdgesCheckBox();
        QVERIFY(!box->isChecked());

        box->setChecked(true);
        QTRY_COMPARE(coreSettings.value(kKey).toString(), QStringLiteral("True"));
        QTRY_VERIFY(coreSlice->limitFiltersToSidebands());
        QVERIFY(core.limitFiltersToSidebands());
        requestLsbAcrossTheCarrier(*coreSlice);
        QCOMPARE(coreSlice->filterHigh(), 0);

        // The Core's own change reaches the window's model and its box.
        coreSettings.setValue(kKey, QStringLiteral("False"));
        QTRY_VERIFY(!coreSlice->limitFiltersToSidebands());
        QTRY_VERIFY(!window.limitFiltersToSidebands());
        QVERIFY(!box->isChecked());
    }

    void removingTheKeyOnTheCoreAppliesTheDefault()
    {
        QTemporaryDir dir;
        AppSettings coreSettings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel core;
        core.addSlice();
        SliceModel* coreSlice = core.slices().first();
        StationServer server(&core, coreSettings,
                             NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));

        coreSettings.setValue(kKey, QStringLiteral("True"));
        QTRY_VERIFY(coreSlice->limitFiltersToSidebands());
        coreSettings.remove(kKey);
        QTRY_VERIFY(!coreSlice->limitFiltersToSidebands());
    }

    void olderCoreRefusalPutsTheBoxBackAndSaysSo()
    {
        RadioModel window(RadioModel::Role::Remote);
        SettingsProxy proxy;
        ScopedRemoteBackend backend(&proxy);
        GeneralFiltersPage page(&window);
        QCheckBox* box = page.limitFilterEdgesCheckBox();
        auto* note = page.findChild<QLabel*>(QStringLiteral("lblLimitFilterEdgesCore"));
        QVERIFY(note != nullptr);
        QVERIFY(note->isHidden());

        box->setChecked(true);
        // An older Core keeps the key to itself: it refuses the write and
        // has no value for it.
        proxy.applyRejection(kKey, QVariant());
        QVERIFY(!box->isChecked());
        QVERIFY(!window.limitFiltersToSidebands());
        QVERIFY(!note->isHidden());
        QVERIFY(OperatorWording::isPlain(note->text()));
        QVERIFY(note->text().contains(QStringLiteral("Core needs updating")));

        note->setVisible(false);
        proxy.applyRejection(QStringLiteral("Region"), QVariant());
        QVERIFY(note->isHidden());
    }
};

QTEST_MAIN(TestLimitFiltersToSidebands)
#include "tst_limit_filters_to_sidebands.moc"
