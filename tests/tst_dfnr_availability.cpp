// no-port-check: NereusSDR-original test.
// =================================================================
// tests/tst_dfnr_availability.cpp  (NereusSDR)
// =================================================================
// R-R3-49, Sub-epic C-1: DFNR runs only while it can, and the VFO flag
// always shows it: disabled, with the plain reason, when it cannot run
// (operator, 2026-09-25: "Not a fan of disappearing buttons but rather
// disabled.").
//
//   - No model (or a build without DFNR): the Core's dfnrRunnable is false
//     with its plain reason, choosing DFNR is refused, and the VFO flag
//     shows the DFNR button disabled with the reason and offers no quick
//     controls.
//   - A model that fails at a channel's first selection: dfnrRunnable goes
//     false, a slice holding DFNR turns it off with the reason, and the
//     button disables.
//   - A remote window follows its Core's mirrored dfnrRunnable.
// No radio is connected and nothing keys.
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original test for NereusSDR by J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code
//               (R-R3-49, Sub-epic C-1).
//   2026-09-25: the button is disabled with the reason, never hidden
//               (tx-followup-3). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include <QtTest>
#include <QApplication>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/ModelPaths.h"
#include "core/WdspTypes.h"
#include "core/dsp/DspAssetService.h"
#include "core/session/StationCapabilities.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "gui/widgets/DspParamPopup.h"
#include "OperatorWording.h"

using namespace NereusSDR;

namespace {

// Shown, enabled, and not carrying a reason.
bool offered(const QPushButton* button)
{
    return button && !button->isHidden() && button->isEnabled();
}

// Shown but disabled, its tooltip the plain reason.
bool shownDisabledWith(const QPushButton* button, const QString& reason)
{
    return button && !button->isHidden() && !button->isEnabled()
        && button->toolTip() == reason && OperatorWording::isPlain(reason);
}

int popupsOf(const QWidget& widget)
{
    return int(widget.findChildren<DspParamPopup*>().size());
}

} // namespace

class TestDfnrAvailability : public QObject {
    Q_OBJECT

private slots:
    void cleanup() { ModelPaths::clearDfnrModelTarballForTest(); }

    void noModelHidesAndRefusesDfnr()
    {
        ModelPaths::setDfnrModelTarballForTest(QString());
        RadioModel model;
        DspAssetService* assets = model.dspAssets();
        QVERIFY(assets);
        QVERIFY(!assets->dfnrRunnable());
#ifdef HAVE_DFNR
        QCOMPARE(assets->dfnrModelStatus(),
                 QStringLiteral("No DFNR model file was found on this Core, so DFNR cannot run."));
#else
        QCOMPARE(assets->dfnrModelStatus(),
                 QStringLiteral("This Core was built without DFNR, so DFNR cannot run."));
#endif
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice);
        QSignalSpy refused(slice, &SliceModel::nrSelectionRefused);
        slice->setActiveNr(NrSlot::DFNR);
        QCOMPARE(slice->activeNr(), NrSlot::Off);
        QCOMPARE(refused.count(), 1);
        QCOMPARE(refused.at(0).at(0).toString(), assets->dfnrModelStatus());
        // Other NR still works.
        slice->setActiveNr(NrSlot::NR2);
        QCOMPARE(slice->activeNr(), NrSlot::NR2);

        VfoWidget vfo;
        vfo.setRadioModel(&model);
        vfo.setSlice(slice);
        QVERIFY(vfo.dfnrButtonForTest());
        QVERIFY(shownDisabledWith(vfo.dfnrButtonForTest(), assets->dfnrModelStatus()));
        // Its quick controls do not open.
        emit vfo.dfnrButtonForTest()->customContextMenuRequested(QPoint(1, 1));
        QCOMPARE(popupsOf(vfo), 0);
    }

#ifdef HAVE_DFNR
    void failedFirstLoadTurnsDfnrOff()
    {
        QTemporaryDir dir;
        const QString tarball = dir.filePath(QStringLiteral("DeepFilterNet3_onnx.tar.gz"));
        ModelPaths::setDfnrModelTarballForTest(tarball);
        RadioModel model;
        DspAssetService* assets = model.dspAssets();
        QVERIFY(assets->dfnrRunnable());
        QVERIFY(assets->dfnrModelStatus().isEmpty());
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice);
        slice->setActiveNr(NrSlot::DFNR);
        QCOMPARE(slice->activeNr(), NrSlot::DFNR);

        VfoWidget vfo;
        vfo.setRadioModel(&model);
        vfo.setSlice(slice);
        QVERIFY(offered(vfo.dfnrButtonForTest()));
        QVERIFY(vfo.dfnrButtonForTest()->isChecked());

        // A channel's first DFNR selection could not load the model.
        QSignalSpy changed(assets, &DspAssetService::dfnrAvailabilityChanged);
        model.reportDfnrUnavailableForTest(/*modelMissing=*/false);
        QCOMPARE(changed.count(), 1);
        QVERIFY(!assets->dfnrRunnable());
        const QString reason = QStringLiteral(
            "The DFNR model file on this Core could not be loaded, so DFNR cannot run.");
        QCOMPARE(assets->dfnrModelStatus(), reason);
        QCOMPARE(slice->activeNr(), NrSlot::Off);
        QCOMPARE(slice->nnrLastError(), reason);
        QVERIFY(shownDisabledWith(vfo.dfnrButtonForTest(), reason));
        QVERIFY(!vfo.dfnrButtonForTest()->isChecked());
        // And it is refused from now on.
        slice->setActiveNr(NrSlot::DFNR);
        QCOMPARE(slice->activeNr(), NrSlot::Off);
    }
#endif

    void remoteWindowFollowsTheCore()
    {
        RadioModel remote(RadioModel::Role::Remote);
        DspAssetService* assets = remote.dspAssets();
        QVERIFY(assets->dfnrRunnable());   // an older Core never says
        VfoWidget vfo;
        vfo.setRadioModel(&remote);
        // Trunk merge (parity Task 16's rule): a Core below dspAssetVersion
        // 3 does not say, so DFNR is shown disabled with that reason, never
        // offered for the Core to refuse.
        QVERIFY(shownDisabledWith(vfo.dfnrButtonForTest(),
                                  RadioModel::noiseReductionNotSaidReason()));
        QVERIFY(OperatorWording::isPlain(RadioModel::noiseReductionNotSaidReason()));
        StationCapabilities caps;
        caps.dspAssetVersion = 2;
        remote.applyStationCapabilities(caps);
        QVERIFY(shownDisabledWith(vfo.dfnrButtonForTest(),
                                  RadioModel::noiseReductionNotSaidReason()));
        caps.dspAssetVersion = 3;
        remote.applyStationCapabilities(caps);
        QVERIFY(offered(vfo.dfnrButtonForTest()));

        const QString coreReason =
            QStringLiteral("No DFNR model file was found on this Core, so DFNR cannot run.");
        QVERIFY(assets->applyRemoteProperty("dfnrModelStatus", coreReason));
        QVERIFY(assets->applyRemoteProperty("dfnrRunnable", false));
        QVERIFY(shownDisabledWith(vfo.dfnrButtonForTest(), coreReason));

        QVERIFY(assets->applyRemoteProperty("dfnrRunnable", true));
        QVERIFY(offered(vfo.dfnrButtonForTest()));
        // A new session starts from the defaults again.
        QVERIFY(assets->applyRemoteProperty("dfnrRunnable", false));
        assets->resetSession();
        QVERIFY(assets->dfnrRunnable());
        QVERIFY(assets->dfnrModelStatus().isEmpty());
        QVERIFY(offered(vfo.dfnrButtonForTest()));
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TestDfnrAvailability test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_dfnr_availability.moc"
