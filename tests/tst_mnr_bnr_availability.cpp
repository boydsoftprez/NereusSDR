// no-port-check: NereusSDR-original test.
// =================================================================
// tests/tst_mnr_bnr_availability.cpp  (NereusSDR)
// =================================================================
// R-R3-49, Sub-epic C-1: MNR is always on the VFO flag. When it cannot
// run, its button is shown disabled with the plain reason, never hidden
// (operator, 2026-09-25: "Not a fan of disappearing buttons but rather
// disabled."), and choosing it is refused with that reason.
//
//   - MNR runs only on a Mac. The Core says whether it can run it
//     (DspAssetService mnrRunnable / mnrStatus); a remote window follows
//     its Core, so a Mac window on a Linux Core shows MNR disabled.
//   - BNR is in no build (NVIDIA only) and is not offered (operator,
//     2026-09-25, tx-followup-4): no flag button, quick controls or DSP
//     menu entry, in a local or a remote window. A BNR selection from any
//     input (the TCI/CAT shim, TCI itself, a saved setting) is refused or
//     ignored, never applied; NrSlot::BNR keeps its value 6.
// No radio is connected and nothing keys.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original test for NereusSDR by J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code
//               (R-R3-49, Sub-epic C-1).
//   2026-09-25: BNR is not offered (tx-followup-4). J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest>
#include <QApplication>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/TciProtocol.h"
#include "core/WdspTypes.h"
#include "core/dsp/DspAssetService.h"
#include "core/session/StationCapabilities.h"
#include "gui/widgets/DspParamPopup.h"
#include "gui/MainWindow.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "OperatorWording.h"

using namespace NereusSDR;

namespace {

bool offered(const QPushButton* button)
{
    return button && !button->isHidden() && button->isEnabled();
}

bool shownDisabledWith(const QPushButton* button, const QString& reason)
{
    return button && !button->isHidden() && !button->isEnabled()
        && button->toolTip() == reason && OperatorWording::isPlain(reason);
}

// No control anywhere in the widget is labelled BNR.
bool offersNoBnr(const QWidget& widget)
{
    for (const QPushButton* button : widget.findChildren<QPushButton*>()) {
        if (button->text().remove(QLatin1Char('&')).contains(QStringLiteral("BNR"))) {
            return false;
        }
        if (button->toolTip().contains(QStringLiteral("NVIDIA"))) {
            return false;
        }
    }
    return true;
}

int popupsOf(const QWidget& widget)
{
    return int(widget.findChildren<DspParamPopup*>().size());
}

const QString kMnrReason =
    QStringLiteral("MNR runs only on a Mac, and this Core is not a Mac, so MNR cannot run.");
const QString kBnrReason =
    QStringLiteral("NVIDIA noise removal is not in this version of NereusSDR.");

} // namespace

class TestMnrBnrAvailability : public QObject {
    Q_OBJECT

private slots:
    void reasonsArePlain()
    {
        QCOMPARE(RadioModel::mnrCannotRunReason(), kMnrReason);
        QCOMPARE(RadioModel::bnrCannotRunReason(), kBnrReason);
        QVERIFY(OperatorWording::isPlain(kMnrReason));
        QVERIFY(OperatorWording::isPlain(kBnrReason));
        QVERIFY(OperatorWording::coreCalledStationIn(kMnrReason).isEmpty());
        QVERIFY(OperatorWording::coreCalledStationIn(kBnrReason).isEmpty());
    }

    void bnrIsNotOfferedAndRefused()
    {
        QCOMPARE(static_cast<int>(NrSlot::BNR), 6);   // the wire value stays
        QVERIFY(!RadioModel::bnrBuilt());
        RadioModel model;
        QCOMPARE(model.nrCannotRunReason(NrSlot::BNR), kBnrReason);
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice);
        QSignalSpy refused(slice, &SliceModel::nrSelectionRefused);
        slice->setActiveNr(NrSlot::BNR);
        QCOMPARE(slice->activeNr(), NrSlot::Off);
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.at(0).at(0).toString(), kBnrReason);

        // The flag in a local window offers no BNR, and no quick controls
        // open from anywhere for it.
        VfoWidget vfo;
        vfo.setRadioModel(&model);
        vfo.setSlice(slice);
        QVERIFY(offersNoBnr(vfo));
        QCOMPARE(popupsOf(vfo), 0);

        // Nor in a remote window.
        RadioModel remote(RadioModel::Role::Remote);
        VfoWidget window;
        window.setRadioModel(&remote);
        QVERIFY(offersNoBnr(window));

        // Nor a flag with no model.
        VfoWidget bare;
        QVERIFY(offersNoBnr(bare));
    }

    void dspMenuOffersNoBnr()
    {
        // The DSP > NR menu is built from these entries in local and remote
        // windows alike.
        const auto entries = MainWindow::nrMenuEntries();
        QCOMPARE(entries.size(), 8);
        for (const auto& entry : entries) {
            QVERIFY(entry.second != NrSlot::BNR);
            QVERIFY(!QString(entry.first).remove(QLatin1Char('&')).contains(QStringLiteral("BNR")));
        }
        // DFNR and MNR stay listed (shown disabled with the reason while
        // they cannot run).
        bool dfnr = false;
        bool mnr = false;
        for (const auto& entry : entries) {
            dfnr = dfnr || entry.second == NrSlot::DFNR;
            mnr = mnr || entry.second == NrSlot::MNR;
        }
        QVERIFY(dfnr);
        QVERIFY(mnr);
    }

    void bnrFromTheTciAndCatShimIsNotApplied()
    {
        RadioModel model;
        const int id = model.addSlice();
        SliceModel* slice = model.sliceById(id);
        QVERIFY(slice);
        // RadioModel::setRxNr's index 5 is BNR (the shim TCI and CAT share).
        model.setRxNr(id, true, 5);
        QCOMPARE(slice->activeNr(), NrSlot::Off);
        QCOMPARE(slice->nnrLastError(), kBnrReason);
        QVERIFY(!model.rxNr(id));
        // TCI itself accepts NR indexes 1..4 only, so 5 is ignored.
        TciProtocol tci(&model);
        tci.handleCommand(QStringLiteral("rx_nr_enable_ex:0,true,5;"));
        QCOMPARE(slice->activeNr(), NrSlot::Off);
        // Other NR still works through the shim.
        model.setRxNr(id, true, 1);
        QCOMPARE(slice->activeNr(), NrSlot::NR2);
    }

    void savedBnrLoadsAsOff()
    {
        const QString mac = QStringLiteral("00:1C:2D:0B:4E:06");
        const QString prefix = QStringLiteral("hardware/") + mac + QStringLiteral("/slices/0/nnr/");
        auto& settings = AppSettings::instance();
        const auto cleanup = qScopeGuard([&settings, mac] {
            for (const QString& key : settings.allKeys()) {
                if (key.startsWith(QStringLiteral("hardware/") + mac)) { settings.remove(key); }
            }
        });
        settings.setValue(prefix + QStringLiteral("NrActive"), static_cast<int>(NrSlot::BNR));
        RadioModel model;
        const int id = model.addSlice();
        SliceModel* slice = model.sliceById(id);
        QVERIFY(slice);
        slice->setSettingsRadioIdentity(mac);
        model.loadSliceState(slice);
        QCOMPARE(slice->activeNr(), NrSlot::Off);
        QVERIFY(!model.rxNr(id));
    }

    void mnrFollowsWhatTheCoreCanRun()
    {
        RadioModel model;
        DspAssetService* assets = model.dspAssets();
        QVERIFY(assets);
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice);
        VfoWidget vfo;
        vfo.setRadioModel(&model);
        vfo.setSlice(slice);
#ifdef HAVE_MNR
        // A Mac Core runs MNR.
        QVERIFY(assets->mnrRunnable());
        QVERIFY(assets->mnrStatus().isEmpty());
        QVERIFY(model.nrCannotRunReason(NrSlot::MNR).isEmpty());
        QVERIFY(offered(vfo.mnrButtonForTest()));
        slice->setActiveNr(NrSlot::MNR);
        QCOMPARE(slice->activeNr(), NrSlot::MNR);
        QVERIFY(vfo.mnrButtonForTest()->isChecked());
        // Were this Core unable to run it, a slice holding MNR turns off
        // with the reason and the button disables.
        assets->setMnrAvailability(false, kMnrReason);
        QCOMPARE(slice->activeNr(), NrSlot::Off);
        QCOMPARE(slice->nnrLastError(), kMnrReason);
        QVERIFY(!vfo.mnrButtonForTest()->isChecked());
#else
        // A Core that is not a Mac cannot.
        QVERIFY(!assets->mnrRunnable());
        QCOMPARE(assets->mnrStatus(), kMnrReason);
#endif
        QCOMPARE(model.nrCannotRunReason(NrSlot::MNR), kMnrReason);
        QVERIFY(shownDisabledWith(vfo.mnrButtonForTest(), kMnrReason));
        emit vfo.mnrButtonForTest()->customContextMenuRequested(QPoint(1, 1));
        QCOMPARE(popupsOf(vfo), 0);
        QSignalSpy refused(slice, &SliceModel::nrSelectionRefused);
        slice->setActiveNr(NrSlot::MNR);
        QCOMPARE(slice->activeNr(), NrSlot::Off);
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.at(0).at(0).toString(), kMnrReason);
        // Other NR still works.
        slice->setActiveNr(NrSlot::NR2);
        QCOMPARE(slice->activeNr(), NrSlot::NR2);
    }

    void remoteWindowFollowsTheCoresMnr()
    {
        RadioModel remote(RadioModel::Role::Remote);
        DspAssetService* assets = remote.dspAssets();
        QVERIFY(assets->mnrRunnable());   // an older Core never says
        VfoWidget vfo;
        vfo.setRadioModel(&remote);
        // Trunk merge (parity Task 16's rule): a Core below dspAssetVersion
        // 4 does not say, so MNR is shown disabled with that reason.
        QVERIFY(shownDisabledWith(vfo.mnrButtonForTest(),
                                  RadioModel::noiseReductionNotSaidReason()));
        StationCapabilities caps;
        caps.dspAssetVersion = 3;
        remote.applyStationCapabilities(caps);
        QVERIFY(shownDisabledWith(vfo.mnrButtonForTest(),
                                  RadioModel::noiseReductionNotSaidReason()));
        QCOMPARE(remote.nrCannotRunReason(NrSlot::MNR),
                 RadioModel::noiseReductionNotSaidReason());
        caps.dspAssetVersion = 4;
        remote.applyStationCapabilities(caps);
        QVERIFY(offered(vfo.mnrButtonForTest()));

        // A Mac window on a Linux Core.
        QVERIFY(assets->applyRemoteProperty("mnrStatus", kMnrReason));
        QVERIFY(assets->applyRemoteProperty("mnrRunnable", false));
        QVERIFY(shownDisabledWith(vfo.mnrButtonForTest(), kMnrReason));
        QCOMPARE(remote.nrCannotRunReason(NrSlot::MNR), kMnrReason);

        QVERIFY(assets->applyRemoteProperty("mnrRunnable", true));
        QVERIFY(offered(vfo.mnrButtonForTest()));
        QVERIFY(remote.nrCannotRunReason(NrSlot::MNR).isEmpty());
        // A new session starts from the defaults again.
        QVERIFY(assets->applyRemoteProperty("mnrRunnable", false));
        QVERIFY(!vfo.mnrButtonForTest()->isEnabled());
        assets->resetSession();
        QVERIFY(assets->mnrRunnable());
        QVERIFY(assets->mnrStatus().isEmpty());
        QVERIFY(offered(vfo.mnrButtonForTest()));
    }

    void flagWithoutAModelFollowsThisBuild()
    {
        VfoWidget vfo;
        QVERIFY(!vfo.mnrButtonForTest()->isHidden());
        QVERIFY(!vfo.dfnrButtonForTest()->isHidden());
        QVERIFY(offersNoBnr(vfo));
#ifdef HAVE_MNR
        QVERIFY(offered(vfo.mnrButtonForTest()));
#else
        QVERIFY(shownDisabledWith(vfo.mnrButtonForTest(), kMnrReason));
#endif
#ifdef HAVE_DFNR
        QVERIFY(offered(vfo.dfnrButtonForTest()));
#else
        QVERIFY(shownDisabledWith(vfo.dfnrButtonForTest(),
                                  RadioModel::nrCannotRunInThisBuildReason(NrSlot::DFNR)));
#endif
        QVERIFY(RadioModel::nrCannotRunInThisBuildReason(NrSlot::NR2).isEmpty());
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TestMnrBnrAvailability test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_mnr_bnr_availability.moc"
