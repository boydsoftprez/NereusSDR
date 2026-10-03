// no-port-check: NereusSDR-original test. The Spectrum Peaks settings are
// stored once for every pan; a Setup change must reach every pan at once,
// not only the pan Setup points at (and not only after a restart).
//
// Modification history (NereusSDR):
//   2026-09-28  J.J. Boyd / KG4VCF  Created. AI-assisted via Anthropic
//                                    Claude Code.

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "gui/ColorSwatchButton.h"
#include "gui/SpectrumWidget.h"
#include "gui/setup/SpectrumPeaksPage.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QSpinBox>

#include <memory>

using namespace NereusSDR;

namespace {

template <typename T>
T* byId(QWidget& page, const char* id)
{
    for (T* widget : page.findChildren<T*>()) {
        if (widget->property("nereusSetupId").toString() == QLatin1String(id)) {
            return widget;
        }
    }
    return nullptr;
}

} // namespace

class TestSpectrumPeaksAllPans : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        auto& s = AppSettings::instance();
        for (const char* key : {"DisplayActivePeakHoldEnabled", "DisplayActivePeakHoldDurationMs",
                                "DisplayActivePeakHoldDropDbPerSec", "DisplayActivePeakHoldFill",
                                "DisplayActivePeakHoldOnTx", "DisplayActivePeakHoldColor",
                                "DisplayPeakBlobsEnabled", "DisplayPeakBlobsCount",
                                "DisplayPeakBlobsInsideFilterOnly", "DisplayPeakBlobsHoldEnabled",
                                "DisplayPeakBlobsHoldMs", "DisplayPeakBlobsHoldDrop",
                                "DisplayPeakBlobsFallDbPerSec", "DisplayPeakBlobColor",
                                "DisplayPeakBlobTextColor"}) {
            s.remove(QLatin1String(key));
        }
    }

    void everyPanFollowsASetupChangeAtOnce()
    {
        RadioModel model;
        SpectrumWidget first;
        SpectrumWidget second;
        auto third = std::make_unique<SpectrumWidget>();
        model.setSpectrumWidget(&first);   // Setup points at the first pan
        SpectrumPeaksPage page(&model);

        for (const SpectrumWidget* pan : {&first, &second, third.get()}) {
            QVERIFY(!pan->activePeakHoldEnabled());
            QVERIFY(!pan->peakBlobsEnabled());
        }

        byId<QCheckBox>(page, "display.spectrumPeaks.activePeakHold")->setChecked(true);
        byId<QSpinBox>(page, "display.spectrumPeaks.activePeakHoldTime")->setValue(700);
        byId<QSpinBox>(page, "display.spectrumPeaks.activePeakHoldDropRate")->setValue(12);
        byId<QCheckBox>(page, "display.spectrumPeaks.activePeakHoldFill")->setChecked(true);
        byId<QCheckBox>(page, "display.spectrumPeaks.activePeakHoldOnTx")->setChecked(true);
        byId<ColorSwatchButton>(page, "display.spectrumPeaks.activePeakHoldColor")
            ->setColor(QColor(0x11, 0x22, 0x33, 0xFF));
        byId<QCheckBox>(page, "display.spectrumPeaks.peakBlobs")->setChecked(true);
        byId<QSpinBox>(page, "display.spectrumPeaks.peakBlobCount")->setValue(7);
        byId<QCheckBox>(page, "display.spectrumPeaks.peakBlobInsideFilter")->setChecked(true);
        byId<QCheckBox>(page, "display.spectrumPeaks.peakBlobHold")->setChecked(true);
        byId<QSpinBox>(page, "display.spectrumPeaks.peakBlobHoldTime")->setValue(900);
        byId<QCheckBox>(page, "display.spectrumPeaks.peakBlobHoldDrop")->setChecked(true);
        byId<QSpinBox>(page, "display.spectrumPeaks.peakBlobFallRate")->setValue(20);
        byId<ColorSwatchButton>(page, "display.spectrumPeaks.peakBlobColor")
            ->setColor(QColor(0x44, 0x55, 0x66, 0xFF));
        byId<ColorSwatchButton>(page, "display.spectrumPeaks.peakBlobTextColor")
            ->setColor(QColor(0x77, 0x88, 0x99, 0xFF));

        for (const SpectrumWidget* pan : {&first, &second, third.get()}) {
            QVERIFY(pan->activePeakHoldEnabled());
            QCOMPARE(pan->activePeakHoldDurationMs(), 700);
            QCOMPARE(pan->activePeakHoldDropDbPerSec(), 12.0);
            QVERIFY(pan->activePeakHoldFill());
            QVERIFY(pan->activePeakHoldOnTx());
            QCOMPARE(pan->activePeakHoldColor(), QColor(0x11, 0x22, 0x33, 0xFF));
            QVERIFY(pan->peakBlobsEnabled());
            QCOMPARE(pan->peakBlobsCount(), 7);
            QVERIFY(pan->peakBlobsInsideFilterOnly());
            QVERIFY(pan->peakBlobsHoldEnabled());
            QCOMPARE(pan->peakBlobsHoldMs(), 900);
            QVERIFY(pan->peakBlobsHoldDrop());
            QCOMPARE(pan->peakBlobsFallDbPerSec(), 20.0);
            QCOMPARE(pan->peakBlobColor(), QColor(0x44, 0x55, 0x66, 0xFF));
            QCOMPARE(pan->peakBlobTextColor(), QColor(0x77, 0x88, 0x99, 0xFF));
        }

        // Switching off reaches every pan too, and a pan that has gone is
        // simply no longer reached.
        third.reset();
        byId<QCheckBox>(page, "display.spectrumPeaks.activePeakHold")->setChecked(false);
        byId<QCheckBox>(page, "display.spectrumPeaks.peakBlobs")->setChecked(false);
        QVERIFY(!first.activePeakHoldEnabled());
        QVERIFY(!second.activePeakHoldEnabled());
        QVERIFY(!first.peakBlobsEnabled());
        QVERIFY(!second.peakBlobsEnabled());
    }

    // A pan made after the change starts with it (it reads the same keys).
    void aNewPanStartsWithTheStoredSettings()
    {
        RadioModel model;
        SpectrumPeaksPage page(&model);
        byId<QCheckBox>(page, "display.spectrumPeaks.peakBlobs")->setChecked(true);
        byId<QSpinBox>(page, "display.spectrumPeaks.peakBlobCount")->setValue(5);
        SpectrumWidget later;
        later.loadSettingsForTest();   // as MainWindow does when it wires a pan
        QVERIFY(later.peakBlobsEnabled());
        QCOMPARE(later.peakBlobsCount(), 5);
    }
};

QTEST_MAIN(TestSpectrumPeaksAllPans)
#include "tst_spectrum_peaks_all_pans.moc"
