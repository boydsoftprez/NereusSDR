// no-port-check: NereusSDR-original test file. It reads the same production
// accessors a local window reads (RxChannel::minNotchWidthHz,
// RxChannel::filterResponseMagnitudes), so a failure means the Core's facts
// did not reach the window, not that this file disagrees with Thetis.
// =================================================================
// tests/tst_remote_dsp_info.cpp  (NereusSDR)
// =================================================================
//
// Remote-window parity Task 16 (R-R3-49, R-R3-21, R-R3-40): DSP facts from
// the Core, under dspInfoVersion 1.
//
//   B2.4  The VFO flag, Setup > DSP > NR/ANF and DSP > NR offer DFNR and MNR
//         by the Core's word, never this computer's build. Since the trunk
//         merge the one source is DspAssetService (dfnrRunnable,
//         mnrRunnable); on a Core below dspAssetVersion 3 (DFNR) or 4 (MNR)
//         they are disabled with a plain reason.
//   B3.4  A remote pan's notch width presets are checked against the Core's
//         minNotchWidthHz, and the TNF page shows it.
//   B3.6  The high-resolution filter graph works in a remote window and
//         draws the Core's curve (dsp.filterResponse), resampled as a local
//         window resamples its own channel's.
//   B3.8  "Time to last change" shows the Core's DSP Options apply time.
//
// Loopback links and a fake radio only: no RF, no audio device, nothing is
// keyed and nothing reaches a real accessory.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26 -- New test file for remote-window parity Task 16. J.J.
//                 Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-26 -- Trunk merge of parity Tasks 16 to 18: the noise reduction
//                 cases read the one source, DspAssetService and
//                 RadioModel::nrCannotRunReason; kNotSaid is the reason
//                 below dspAssetVersion 3 or 4. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27 -- R-R3-49: the Core's receive lane is waited for as such
//                 before its results are read. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QAction>
#include <QCheckBox>
#include <QCoreApplication>
#include <QFile>
#include <QLabel>
#include <QLoggingCategory>
#include <QMetaProperty>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <cmath>
#include <memory>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/HardwareProfile.h"
#include "core/RxChannel.h"
#include "core/WdspEngine.h"
#include "core/dsp/DspAssetService.h"
#include "core/session/IStationLink.h"
#include "core/session/MirrorPolicy.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/ConnectableRadioModel.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/RemoteWindowHarness.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/SpectrumWidget.h"
#include "gui/setup/DspOptionsPage.h"
#include "gui/setup/DspSetupPages.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "OperatorWording.h"

using namespace NereusSDR;
using NereusSDR::Test::ConnectableRadioModel;
using NereusSDR::Test::LoopbackTransport;
using NereusSDR::Test::RemoteWindowHarness;

namespace {

const QString kNotSaid = QStringLiteral(
    "This Core does not say which noise reduction it can run. Updating the Core may help.");

std::unique_ptr<RadioModel> makeStationRadioModel()
{
    auto model = std::make_unique<RadioModel>();
    model->setBoardForTest(HPSDRHW::HermesLite);
    RadioInfo info;
    info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:16");
    info.name = QStringLiteral("Bench HL2");
    info.boardType = HPSDRHW::HermesLite;
    model->setLastRadioInfoForTest(info);
    model->setConnectionStateForTest(ConnectionState::Connected);
    model->addSlice(QStringLiteral("pan-0"));
    return model;
}

// A Core and one window over the loopback, handshake complete. `core` is
// the Core's radio model (a plain one, or a ConnectableRadioModel's with a
// live channel).
struct Session {
    Session(RadioModel* core, const QString& securityDir, QObject* parent)
        : settings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")))
        , coreModel(core)
    {
        server = std::make_unique<StationServer>(
            coreModel, settings, NereusSDR::Test::seedUpgradedCoreToken(securityDir));
        client = std::make_unique<StationClient>(&window, &proxy);
        coreEnd = new LoopbackTransport(QStringLiteral("station-end"), parent);
        windowEnd = new LoopbackTransport(QStringLiteral("client-end"), parent);
        coreEnd->linkTo(windowEnd);
    }
    bool connect()
    {
        QSignalSpy completed(client.get(), &StationClient::handshakeComplete);
        client->startSession(windowEnd, server->token());
        server->acceptTransport(coreEnd);
        return completed.wait(5000) || completed.count() == 1;
    }
    // The command.result the Core sent for `verb` invoked as `id`.
    std::optional<SessionMessage> resultFor(const QByteArray& verb, quint32 id) const
    {
        for (const QByteArray& wire : windowEnd->received()) {
            SessionMessage message;
            if (SessionMessages::decode(wire, &message)
                && message.kind == SessionMessageKind::CommandResult
                && message.commandVerb == verb && message.commandId == id) {
                return message;
            }
        }
        return std::nullopt;
    }
    void invoke(const QByteArray& verb, quint32 id, const QList<MirrorUpdate>& args)
    {
        windowEnd->sendText(SessionMessages::encode(
            SessionMessages::commandInvoke(verb, id, args)));
    }

    QTemporaryDir settingsDir;
    AppSettings settings;
    RadioModel* coreModel = nullptr;
    std::unique_ptr<StationServer> server;
    RadioModel window{RadioModel::Role::Remote};
    SettingsProxy proxy;
    std::unique_ptr<StationClient> client;
    LoopbackTransport* coreEnd = nullptr;
    LoopbackTransport* windowEnd = nullptr;
};

QVariant valueOf(const SessionMessage& message, const QByteArray& name)
{
    for (const MirrorUpdate& u : message.updates) {
        if (u.name == name) {
            return u.value;
        }
    }
    return {};
}

const QString kMnrNotAMac = QStringLiteral(
    "MNR runs only on a Mac, and this Core is not a Mac, so MNR cannot run.");
const QString kDfnrNoModel = QStringLiteral(
    "No DFNR model file was found on this Core, so DFNR cannot run.");

} // namespace

class TstRemoteDspInfo : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false"));
        QVERIFY(m_securityDir.isValid());
        QVERIFY(RemoteWindowHarness::useIsolatedProfile(QStringLiteral("remote-dsp-info")));
        AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"),
                                         QStringLiteral("7"));
    }

    void cleanupTestCase() { QVERIFY(RemoteWindowHarness::removeIsolatedProfile()); }

    // The two properties are Core to window only, with no WRITE. Which
    // noise reduction runs has one source, DspAssetService: the radio
    // object carries no list of its own.
    void factsAreOutbound()
    {
        RadioModel radio;
        QCOMPARE(radio.metaObject()->indexOfProperty("noiseReductionMethods"), -1);
        const struct { const QMetaObject* mo; const char* cls; const char* name; int type; } props[] = {
            {radio.metaObject(), "RadioModel", "dspOptionsLastApplyMs", QMetaType::LongLong},
            {&SliceModel::staticMetaObject, "SliceModel", "minNotchWidthHz", QMetaType::Double},
        };
        for (const auto& p : props) {
            const int index = p.mo->indexOfProperty(p.name);
            QVERIFY2(index >= 0, p.name);
            const QMetaProperty prop = p.mo->property(index);
            QVERIFY2(!prop.isWritable(), p.name);
            QVERIFY2(prop.hasNotifySignal(), p.name);
            QCOMPARE(prop.metaType().id(), p.type);
            QCOMPARE(MirrorPolicy::directionFor(QByteArray(p.cls), QByteArray(p.name)),
                     MirrorDirection::Outbound);
        }
    }

    // What a computer can run is its own build's and platform's: MNR only
    // with HAVE_MNR, DFNR only with HAVE_DFNR.
    void localNoiseReductionFollowsTheBuild()
    {
#ifdef HAVE_MNR
        QVERIFY(RadioModel::nrCannotRunInThisBuildReason(NrSlot::MNR).isEmpty());
#else
        QCOMPARE(RadioModel::nrCannotRunInThisBuildReason(NrSlot::MNR), kMnrNotAMac);
#endif
#ifndef HAVE_DFNR
        QVERIFY(!RadioModel::nrCannotRunInThisBuildReason(NrSlot::DFNR).isEmpty());
#endif
        RadioModel local;
        QVERIFY(local.nrCannotRunReason(NrSlot::NR2).isEmpty());
        // A local model is never "not told".
        QVERIFY(local.nrCannotRunReason(NrSlot::MNR) != kNotSaid);
        QVERIFY(local.nrCannotRunReason(NrSlot::DFNR) != kNotSaid);
    }

    // The Core's DspAssetService word reaches the window over a real
    // session, with dspAssetVersion 4, so the window is told.
    void coreSaysWhichNoiseReductionItRuns()
    {
        std::unique_ptr<RadioModel> core = makeStationRadioModel();
        Session s(core.get(), m_securityDir.path(), this);
        QCOMPARE(s.server->dspInfoVersion(), 1);
        QVERIFY(s.connect());
        QCOMPARE(s.client->capabilities().dspInfoVersion, 1);
        QVERIFY(s.client->capabilities().dspAssetVersion >= 4);
        QCOMPARE(s.window.stationDspInfoVersion(), 1);
        QVERIFY(s.window.stationDspAssetVersion() >= 4);
        QTRY_COMPARE(s.window.dspAssets()->mnrRunnable(), core->dspAssets()->mnrRunnable());
        QTRY_COMPARE(s.window.dspAssets()->dfnrRunnable(), core->dspAssets()->dfnrRunnable());
        QCOMPARE(s.window.nrCannotRunReason(NrSlot::MNR).isEmpty(),
                 core->dspAssets()->mnrRunnable());
        QVERIFY(s.window.nrCannotRunReason(NrSlot::MNR) != kNotSaid);
        QVERIFY(s.window.nrCannotRunReason(NrSlot::DFNR) != kNotSaid);

        // A Core with no local radio model offers 0.
        RadioModel remote(RadioModel::Role::Remote);
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("NereusSDR.settings")));
        StationServer server(&remote, settings,
                             NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
        QCOMPARE(server.dspInfoVersion(), 0);
    }

    // B2.4: the Core decides, never this computer's build. A Mac window on
    // a Linux Core offers no MNR; a Linux window on a Mac Core offers it.
    // On a Core too old to say, both are disabled with the plain reason.
    void flagAndPagesOfferWhatTheCoreRuns()
    {
        RadioModel window(RadioModel::Role::Remote);
        DspAssetService* assets = window.dspAssets();
        QVERIFY(assets);
        VfoWidget flag;
        flag.setRadioModel(&window);
        QPushButton* mnr = flag.mnrButtonForTest();
        QPushButton* dfnr = flag.dfnrButtonForTest();
        QVERIFY(mnr && dfnr);

        // A Core that does not say: shown, disabled, with the reason.
        QVERIFY(!mnr->isHidden());
        QVERIFY(!mnr->isEnabled());
        QVERIFY(!dfnr->isEnabled());
        QCOMPARE(mnr->toolTip(), kNotSaid);
        QCOMPARE(dfnr->toolTip(), kNotSaid);
        QVERIFY(OperatorWording::isPlain(kNotSaid));

        // A Linux Core without the DFNR model: no MNR, no DFNR.
        StationCapabilities caps;
        caps.dspInfoVersion = 1;
        caps.dspAssetVersion = 4;
        window.applyStationCapabilities(caps);
        QVERIFY(assets->applyRemoteProperty("mnrStatus", kMnrNotAMac));
        QVERIFY(assets->applyRemoteProperty("mnrRunnable", false));
        QVERIFY(assets->applyRemoteProperty("dfnrModelStatus", kDfnrNoModel));
        QVERIFY(assets->applyRemoteProperty("dfnrRunnable", false));
        QVERIFY(!mnr->isEnabled());
        QCOMPARE(mnr->toolTip(), kMnrNotAMac);
        QVERIFY(!dfnr->isEnabled());
        QCOMPARE(dfnr->toolTip(), kDfnrNoModel);
        QVERIFY(OperatorWording::isPlain(mnr->toolTip()));
        QVERIFY(OperatorWording::isPlain(dfnr->toolTip()));

        // A Mac Core with DFNR: both offered, whatever this build has.
        QVERIFY(assets->applyRemoteProperty("mnrStatus", QString()));
        QVERIFY(assets->applyRemoteProperty("mnrRunnable", true));
        QVERIFY(assets->applyRemoteProperty("dfnrModelStatus", QString()));
        QVERIFY(assets->applyRemoteProperty("dfnrRunnable", true));
        QVERIFY(mnr->isEnabled());
        QVERIFY(dfnr->isEnabled());
        QVERIFY(mnr->toolTip() != kNotSaid && mnr->toolTip() != kMnrNotAMac);

        // Setup > DSP > NR/ANF follows the same word.
        NrAnfSetupPage page(&window);
        auto* mnrNote = page.findChild<QLabel*>(QStringLiteral("mnrUnavailableNote"));
        auto* dfnrNote = page.findChild<QLabel*>(QStringLiteral("dfnrUnavailableNote"));
        QVERIFY(mnrNote && dfnrNote);
        QVERIFY(mnrNote->isHidden());
        QVERIFY(mnrNote->parentWidget() != nullptr);
        QVERIFY(assets->applyRemoteProperty("mnrStatus", kMnrNotAMac));
        QVERIFY(assets->applyRemoteProperty("mnrRunnable", false));
        QVERIFY(assets->applyRemoteProperty("dfnrModelStatus", kDfnrNoModel));
        QVERIFY(assets->applyRemoteProperty("dfnrRunnable", false));
        QVERIFY(!mnrNote->isHidden());
        QCOMPARE(mnrNote->text(), kMnrNotAMac);
        QCOMPARE(dfnrNote->text(), kDfnrNoModel);
        QVERIFY(!mnr->isEnabled());

        // And the page, like the flag, says so on a Core too old to say.
        caps.dspAssetVersion = 2;
        window.applyStationCapabilities(caps);
        QCOMPARE(mnrNote->text(), kNotSaid);
        QCOMPARE(dfnrNote->text(), kNotSaid);
        QCOMPARE(mnr->toolTip(), kNotSaid);
        QCOMPARE(dfnr->toolTip(), kNotSaid);
    }

    // B3.8: the Core's apply time reaches the window and its page.
    void timeToLastChangeIsTheCores()
    {
        std::unique_ptr<RadioModel> core = makeStationRadioModel();
        Session s(core.get(), m_securityDir.path(), this);
        QVERIFY(s.connect());
        QCOMPARE(s.window.dspOptionsLastApplyMs(), 0);
        DspOptionsPage page(&s.window);
        auto* label = [&page]() -> QLabel* {
            for (QLabel* l : page.findChildren<QLabel*>()) {
                if (l->text().startsWith(QStringLiteral("Time to last change"))) {
                    return l;
                }
            }
            return nullptr;
        }();
        QVERIFY(label);
        QCOMPARE(label->text(), QStringLiteral("Time to last change: none"));

        emit core->dspChangeMeasured(37);
        QCOMPARE(core->dspOptionsLastApplyMs(), 37);
        QTRY_COMPARE(s.window.dspOptionsLastApplyMs(), 37);
        QCOMPARE(label->text(), QStringLiteral("Time to last change: 37 ms"));
    }

    // B3.4 on the Core: each slice carries its channel's minimum, as the
    // local TNF page reads it.
    void minNotchWidthFollowsTheCoresChannel()
    {
        std::unique_ptr<ConnectableRadioModel> harness = ConnectableRadioModel::create();
        QVERIFY(harness != nullptr);
        RadioModel& model = harness->model();
        SliceModel* slice = model.sliceById(0);
        QVERIFY(slice != nullptr);
        RxChannel* ch = model.wdspEngine()->rxChannel(slice->sliceIndex());
        QVERIFY(ch != nullptr);
        QVERIFY(ch->minNotchWidthHz() > 0.0);
        QTRY_COMPARE(slice->minNotchWidthHz(), ch->minNotchWidthHz());

        // A larger filter size narrows it (nbp.c min_notch_width), and the
        // slice follows the channel's signal.
        const double before = ch->minNotchWidthHz();
        ch->setFilterSizeSamples(8192);
        // Trunk merge: the WDSP call runs on the receive lane (R-R3-39), so
        // the new minimum lands once the lane has run it. R-R3-49: waited
        // for as that, not for QTRY's 5 s: the rebuild at 8192 samples took
        // longer than that at load 430.
        QVERIFY(model.waitForReceiveLaneForTest());
        QVERIFY(ch->minNotchWidthHz() < before);
        QTRY_COMPARE(slice->minNotchWidthHz(), ch->minNotchWidthHz());
        harness.reset();
    }

    // B3.4 in the window: the TNF page shows the Core's minimum.
    void tnfPageShowsTheCoresMinimum()
    {
        std::unique_ptr<RadioModel> core = makeStationRadioModel();
        Session s(core.get(), m_securityDir.path(), this);
        QVERIFY(s.connect());
        SliceModel* coreSlice = core->slices().first();
        QTRY_VERIFY(s.window.sliceById(coreSlice->sliceIndex()) != nullptr);
        SliceModel* windowSlice = s.window.sliceById(coreSlice->sliceIndex());
        QTRY_VERIFY(s.window.activeSlice() != nullptr);

        MnfSetupPage page(&s.window);
        page.show();
        auto* readout = page.findChild<QLabel*>(QStringLiteral("lblMNFMinWidth"));
        QVERIFY(readout);
        QCOMPARE(readout->text(), QStringLiteral("--"));
        coreSlice->setMinNotchWidthHz(50.0);
        QTRY_COMPARE(windowSlice->minNotchWidthHz(), 50.0);
        QTRY_COMPARE(readout->text(), QStringLiteral("50.0 Hz"));
    }

    // B3.4 through a real remote window: the pan's presets follow the
    // Core's minimum, and DSP > NR offers MNR by the Core's word.
    void remotePanPresetsFollowTheCoresMinimum()
    {
        RemoteWindowHarness h;
        QVERIFY(h.start());
        QAction* connect = h.menuAction(QStringLiteral("&Radio"), QStringLiteral("&Connect"));
        QVERIFY(connect && connect->isEnabled());
        connect->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(h.client()->isHandshakeComplete(), 10000);
        QCOMPARE(h.client()->capabilities().dspInfoVersion, 1);
        SpectrumWidget* pan = h.panSpectrum(QStringLiteral("pan-0"));
        QVERIFY(pan);
        SliceModel* coreSlice = h.station().slices().first();
        coreSlice->setMinNotchWidthHz(25.0);
        QTRY_COMPARE(pan->notchMinWidthHzForTest(), 25.0);
        coreSlice->setMinNotchWidthHz(200.0);
        QTRY_COMPARE(pan->notchMinWidthHzForTest(), 200.0);

        QTRY_COMPARE(h.remoteModel()->dspAssets()->mnrRunnable(),
                     h.station().dspAssets()->mnrRunnable());
    }

    // B3.6 on the wire: dsp.filterResponse answers with the Core's bins;
    // resampled in the window they are the curve the Core's own graph
    // draws. Read either way; an unreadable request or an unknown slice is
    // refused with the Core's words.
    void filterResponseIsTheCoresCurve()
    {
        std::unique_ptr<ConnectableRadioModel> harness = ConnectableRadioModel::create();
        QVERIFY(harness != nullptr);
        RadioModel& core = harness->model();
        Session s(&core, m_securityDir.path(), this);
        QVERIFY(s.connect());
        QCOMPARE(s.client->capabilities().dspInfoVersion, 1);
        SliceModel* coreSlice = core.sliceById(0);
        QVERIFY(coreSlice);
        RxChannel* ch = core.wdspEngine()->rxChannel(coreSlice->sliceIndex());
        QVERIFY(ch);
        QTRY_VERIFY(s.window.sliceById(0) != nullptr);

        QSignalSpy curve(&s.window, &RadioModel::coreFilterResponseChanged);
        s.window.setCoreFilterResponseWanted(true);
        QTRY_COMPARE(curve.count(), 1);
        const RadioModel::FilterResponse& response = s.window.coreFilterResponse();
        double stepHz = 0.0;
        const QVector<double> bins = ch->filterResponseBins(&stepHz);
        QCOMPARE(response.magnitudesDb.size(), bins.size());
        QCOMPARE(response.stepHz, stepHz);
        QCOMPARE(response.startHz, 0.0);

        // Drawn at a graph's width in the window, as the Core draws it.
        QVector<double> linear;
        for (double db : response.magnitudesDb) {
            linear.append(std::pow(10.0, db / 20.0));
        }
        for (int width : {137, 300, 811}) {
            const QVector<float> window = RxChannel::resampleFilterResponse(linear, width);
            const QVector<float> local = ch->filterResponseMagnitudes(width);
            QCOMPARE(window.size(), local.size());
            for (int i = 0; i < width; ++i) {
                // The graph shows 0 to -80 dB; the wire rounds to 0.001 dB.
                if (local[i] > -80.0f) {
                    QVERIFY2(std::abs(window[i] - local[i]) < 0.01f,
                             qPrintable(QStringLiteral("x=%1 window %2 local %3")
                                            .arg(i).arg(window[i]).arg(local[i])));
                }
            }
        }

        // A filter change on the Core's slice is fetched again. R-R3-49:
        // the change runs on the Core's receive lane first, waited for as
        // that rather than inside QTRY's 5 s.
        coreSlice->setFilter(300, 2400);
        QVERIFY(core.waitForReceiveLaneForTest());
        QTRY_VERIFY(curve.count() >= 2);

        // Right and wrong on the wire.
        s.invoke("dsp.filterResponse", 9001,
                 {{0, "sliceId", MirrorWireKind::Int64, qlonglong(0)},
                  {0, "highResolution", MirrorWireKind::Bool, false}});
        QTRY_VERIFY(s.resultFor("dsp.filterResponse", 9001).has_value());
        std::optional<SessionMessage> plain = s.resultFor("dsp.filterResponse", 9001);
        QVERIFY(plain->accepted);
        QCOMPARE(valueOf(*plain, "magnitudesDbJson").toString(), QStringLiteral("[]"));
        s.invoke("dsp.filterResponse", 9002,
                 {{0, "sliceId", MirrorWireKind::Int64, qlonglong(42)},
                  {0, "highResolution", MirrorWireKind::Bool, true}});
        QTRY_VERIFY(s.resultFor("dsp.filterResponse", 9002).has_value());
        std::optional<SessionMessage> unknown = s.resultFor("dsp.filterResponse", 9002);
        QVERIFY(!unknown->accepted);
        QCOMPARE(unknown->reason, QStringLiteral("The Core has no such slice."));
        s.invoke("dsp.filterResponse", 9003,
                 {{0, "sliceId", MirrorWireKind::Int64, qlonglong(0)},
                  {0, "wide", MirrorWireKind::Bool, true}});
        QTRY_VERIFY(s.resultFor("dsp.filterResponse", 9003).has_value());
        std::optional<SessionMessage> unread = s.resultFor("dsp.filterResponse", 9003);
        QVERIFY(!unread->accepted);
        QCOMPARE(unread->reason, QStringLiteral("The Core could not read this request."));
        harness.reset();
    }

    // With highResolution false the Core takes the request and sends no
    // curve.
    void filterResponseWithoutHighResolutionIsEmpty()
    {
        std::unique_ptr<RadioModel> core = makeStationRadioModel();
        RadioModel::FilterResponse response;
        response.stepHz = 1.0;
        QString reason;
        QVERIFY(core->filterResponseForStation(0, false, &response, &reason));
        QVERIFY(response.magnitudesDb.isEmpty());
        QCOMPARE(response.stepHz, 0.0);
        QCOMPARE(RadioModel::filterResponseToJson(response.magnitudesDb), QStringLiteral("[]"));
        // With no receiver running, a curve is refused in plain words.
        QVERIFY(!core->filterResponseForStation(0, true, &response, &reason));
        QVERIFY(OperatorWording::isPlain(reason));
        // The JSON round trip keeps the values to 0.001 dB.
        const QVector<double> values{0.0, -3.0104, -80.12345, -120.0};
        const std::optional<QVector<double>> back =
            RadioModel::filterResponseFromJson(RadioModel::filterResponseToJson(values));
        QVERIFY(back.has_value());
        QCOMPARE(back->size(), values.size());
        for (int i = 0; i < values.size(); ++i) {
            QVERIFY(std::abs(back->at(i) - values[i]) <= 0.0005);
        }
        QVERIFY(!RadioModel::filterResponseFromJson(QStringLiteral("{}")).has_value());
        QVERIFY(!RadioModel::filterResponseFromJson(QStringLiteral("[\"x\"]")).has_value());
    }

    // B3.6 on the page: enabled in a remote window whose Core sends the
    // curve; disabled with the reason on one that does not.
    void highResolutionBoxFollowsTheCore()
    {
        RadioModel window(RadioModel::Role::Remote);
        DspOptionsPage page(&window);
        QCheckBox* highRes = page.highResolutionFilterCharacteristicsCheckBox();
        QVERIFY(highRes);
        QVERIFY(!highRes->isEnabled());
        QCOMPARE(highRes->toolTip(), IStationLink::filterResponseUnavailableReason());
        QVERIFY(OperatorWording::isPlain(highRes->toolTip()));

        StationCapabilities caps;
        caps.dspInfoVersion = 1;
        window.applyStationCapabilities(caps);
        QVERIFY(highRes->isEnabled());
        QVERIFY(highRes->toolTip().startsWith(QStringLiteral("When enabled")));
    }

private:
    QTemporaryDir m_securityDir;
};

QTEST_MAIN(TstRemoteDspInfo)
#include "tst_remote_dsp_info.moc"
