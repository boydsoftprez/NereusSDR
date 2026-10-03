// no-port-check: NereusSDR-original Setup description versus desktop widgets.
#include <QtTest>

#include "core/setup/SetupDescriptionService.h"
#include "core/AppSettings.h"
#include "core/ControlRanges.h"
#include "core/TxAnalyzer.h"
#include "core/StepAttenuatorController.h"
#include "core/settings/SettingsScope.h"
#include "gui/setup/GeneralOptionsPage.h"
#include "gui/setup/AppearanceSetupPages.h"
#include "gui/SMeterWidget.h"
#include "gui/ColorSwatchButton.h"
#include "gui/setup/GeneralSetupPages.h"
#include "gui/setup/CatNetworkSetupPages.h"
#include "gui/setup/FourO3APage.h"
#include "gui/setup/RfKitPage.h"
#include "gui/setup/DspSetupPages.h"
#include "gui/setup/DspOptionsPage.h"
#include "gui/setup/FilterPresetsSetupPage.h"
#include "gui/setup/DisplaySetupPages.h"
#include "gui/setup/MultimeterPage.h"
#include "gui/setup/SpectrumPeaksPage.h"
#include "gui/setup/TransmitSetupPages.h"
#include "gui/setup/TxProfileSetupPage.h"
#include "gui/setup/hardware/AntennaAlexAlex1Tab.h"
#include "gui/setup/hardware/AntennaAlexAlex2Tab.h"
#include "gui/setup/AudioTxInputPage.h"
#include "gui/setup/hardware/AntennaAlexAntennaControlTab.h"
#include "gui/setup/hardware/CalibrationTab.h"
#include "gui/setup/hardware/Hl2IoBoardTab.h"
#include "gui/setup/hardware/Hl2OptionsTab.h"
#include "gui/setup/hardware/RadioInfoTab.h"
#include "core/PaCalProfile.h"
#include "core/codec/AlexFilterMap.h"
#include "core/RadioDiscovery.h"
#include "gui/setup/PaSetupPages.h"
#include "gui/widgets/MetricLabel.h"
#include "gui/setup/TestTwoTonePage.h"
#include "gui/diagnostics/DiagnosticsPhaseHPages.h"
#include "gui/diagnostics/RadioStatusPage.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/NotchModel.h"
#include "models/Band.h"
#include "models/PanadapterModel.h"
#include "gui/SpectrumWidget.h"
#include "core/spectrum/DisplayFollowers.h"

#include <QAbstractButton>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QGroupBox>
#include <QPushButton>
#include <QGridLayout>
#include <QHostAddress>
#include <QFormLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QSlider>
#include <QTableWidget>

#include <utility>

using namespace NereusSDR;

namespace {
QJsonArray controls(const QJsonObject& category)
{
    QJsonArray result;
    for (const QJsonValue& page : category.value("pages").toArray()) {
        for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
            for (const QJsonValue& control : section.toObject().value("controls").toArray()) {
                result.append(control);
            }
        }
    }
    return result;
}

QObject* bySetupId(QWidget& page, const QString& id)
{
    for (QObject* object : page.findChildren<QObject*>()) {
        if (object->property("nereusSetupId").toString() == id) { return object; }
    }
    // Version 15: one desktop widget that serves several described rows.
    for (QObject* object : page.findChildren<QObject*>()) {
        if (object->property("nereusSetupIds").toStringList().contains(id)) { return object; }
    }
    return nullptr;
}

QJsonObject projectedCategory(const QString& description, int version)
{
    return QJsonDocument::fromJson(
        SetupDescriptionService::fitCategoryForVersion(description, version).toUtf8()).object();
}

// The text a described row is labelled with on the desktop: its form label,
// else the QLabel just before it in a box row, else its accessible name.
QString nativeRowLabel(QWidget& page, QWidget* widget)
{
    QWidget* field = widget;
    for (QWidget* parent = widget->parentWidget(); parent; parent = parent->parentWidget()) {
        if (auto* form = qobject_cast<QFormLayout*>(parent->layout())) {
            auto* label = qobject_cast<QLabel*>(form->labelForField(field));
            if (label && !label->text().isEmpty()) { return label->text(); }
        }
        field = parent;
    }
    for (QBoxLayout* box : page.findChildren<QBoxLayout*>()) {
        for (int i = 1; i < box->count(); ++i) {
            if (box->itemAt(i)->widget() != widget) { continue; }
            if (auto* label = qobject_cast<QLabel*>(box->itemAt(i - 1)->widget())) {
                return label->text();
            }
        }
    }
    return widget->accessibleName();
}

void compareControl(QWidget& page, const QJsonObject& control)
{
    const QString id = control.value("id").toString();
    QObject* object = bySetupId(page, id);
    QVERIFY2(object != nullptr, qPrintable(id + " has no desktop widget"));
    const QString kind = control.value("kind").toString();
    if (kind == "toggle" || kind == "button") {
        auto* button = qobject_cast<QAbstractButton*>(object);
        QVERIFY2(button != nullptr, qPrintable(id));
        QCOMPARE(button->text(), control.value("label").toString());
    } else if (control.contains("rangeFrom")) {
        // Version 15: the range and shown values are the catalogue's
        // (tst_catalogue_ranges holds them against the widget).
        QVERIFY2(qobject_cast<QAbstractSpinBox*>(object) != nullptr
                     || qobject_cast<QSlider*>(object) != nullptr, qPrintable(id));
    } else if (kind == "integer") {
        auto* spin = qobject_cast<QSpinBox*>(object);
        QVERIFY2(spin != nullptr, qPrintable(id));
        QCOMPARE(spin->minimum(), control.value("min").toInt());
        QCOMPARE(spin->maximum(), control.value("max").toInt());
        QCOMPARE(spin->singleStep(), control.value("step").toInt());
    } else if (kind == "decimal" || kind == "slider") {
        if (kind == "slider" && control.contains("options")) {
            auto* slider = qobject_cast<QSlider*>(object);
            QVERIFY2(slider != nullptr, qPrintable(id));
            const QJsonArray options = control.value("options").toArray();
            QCOMPARE(slider->minimum(), 0);
            QCOMPARE(slider->maximum(), options.size() - 1);
            QCOMPARE(slider->singleStep(), 1);
            for (int i = 0; i < options.size(); ++i) {
                const int value = 4096 << i;
                QCOMPARE(options.at(i).toObject().value("value"), QJsonValue(value));
                QCOMPARE(options.at(i).toObject().value("label"), QJsonValue(QString::number(value)));
            }
        } else if (auto* spin = qobject_cast<QDoubleSpinBox*>(object)) {
            QCOMPARE(spin->minimum(), control.value("min").toDouble());
            QCOMPARE(spin->maximum(), control.value("max").toDouble());
            QCOMPARE(spin->singleStep(), control.value("step").toDouble());
        } else if (auto* spin = qobject_cast<QSpinBox*>(object)) {
            QCOMPARE(double(spin->minimum()), control.value("min").toDouble());
            QCOMPARE(double(spin->maximum()), control.value("max").toDouble());
            QCOMPARE(double(spin->singleStep()), control.value("step").toDouble());
        } else {
            auto* slider = qobject_cast<QSlider*>(object);
            QVERIFY2(slider != nullptr, qPrintable(id));
            const double scale = slider->property("nereusSetupScale").isValid()
                ? slider->property("nereusSetupScale").toDouble() : 1.0;
            QCOMPARE(slider->minimum() / scale, control.value("min").toDouble());
            QCOMPARE(slider->maximum() / scale, control.value("max").toDouble());
            QCOMPARE(slider->singleStep() / scale, control.value("step").toDouble());
        }
    } else if (kind == "choice") {
        if (auto* combo = qobject_cast<QComboBox*>(object)) {
            const QJsonArray choices = control.contains("options")
                ? control.value("options").toArray() : control.value("choices").toArray();
            QCOMPARE(combo->count(), choices.size());
            for (int i = 0; i < choices.size(); ++i) {
                QCOMPARE(combo->itemText(i), control.contains("options")
                    ? choices.at(i).toObject().value("label").toString()
                    : choices.at(i).toString());
            }
        } else {
            auto* group = qobject_cast<QButtonGroup*>(object);
            QVERIFY2(group != nullptr, qPrintable(id));
            const QJsonArray choices = control.value("choices").toArray();
            const QJsonArray options = control.value("options").toArray();
            QCOMPARE(group->buttons().size(), options.isEmpty() ? choices.size() : options.size());
            for (int i = 0; i < group->buttons().size(); ++i) {
                auto* button = group->button(i);
                QVERIFY(button != nullptr);
                QCOMPARE(button->text(), options.isEmpty() ? choices.at(i).toString()
                                                           : options.at(i).toObject().value("label").toString());
            }
        }
    } else if (kind == "table" && id == "dsp.cfc.bands") {
        // Version 19: the band editor is the desktop's Configure CFC bands
        // button, which opens the same editor (TxCfcDialog).
        auto* button = qobject_cast<QAbstractButton*>(object);
        QVERIFY2(button != nullptr, qPrintable(id));
        QCOMPARE(button->text(), control.value("label").toString());
    } else if (kind == "table" && id == "dsp.filterPresets.presets") {
        // Version 15: the Filter Presets table's columns, in the desktop's order.
        auto* table = qobject_cast<QTableWidget*>(object);
        QVERIFY2(table != nullptr, qPrintable(id));
        const QJsonArray columns = control.value("columns").toArray();
        QCOMPARE(table->columnCount(), columns.size());
        for (int col = 0; col < columns.size(); ++col) {
            QCOMPARE(table->horizontalHeaderItem(col)->text(),
                     columns.at(col).toObject().value("label").toString());
        }
        QVERIFY(table->rowCount() > 0);
        auto* name = qobject_cast<QLineEdit*>(table->cellWidget(0, 1));
        QVERIFY(name != nullptr);
        QCOMPARE(name->maxLength(), columns.at(1).toObject().value("maxLength").toInt());
        for (int col : {2, 3}) {
            auto* spin = qobject_cast<QSpinBox*>(table->cellWidget(0, col));
            QVERIFY(spin != nullptr);
            QCOMPARE(spin->minimum(), columns.at(col).toObject().value("min").toInt());
            QCOMPARE(spin->maximum(), columns.at(col).toObject().value("max").toInt());
        }
    } else if (kind == "table") {
        auto* table = qobject_cast<QTableWidget*>(object);
        QVERIFY2(table != nullptr, qPrintable(id));
        auto* group = qobject_cast<QGroupBox*>(table->parentWidget());
        QVERIFY(group != nullptr);
        QCOMPARE(group->title(), control.value("label").toString());
        QCOMPARE(table->rowCount(), 1);
        const QJsonArray columns = control.value("columns").toArray();
        QCOMPARE(table->columnCount(), columns.size());
        for (int col = 0; col < columns.size(); ++col) {
            const QJsonObject column = columns.at(col).toObject();
            QWidget* cell = table->cellWidget(0, col);
            QVERIFY(cell != nullptr);
            QCOMPARE(cell->property("nereusSetupId").toString(), column.value("id").toString());
            QCOMPARE(cell->toolTip(), column.value("tooltip").toString());
            const QString header = col == 3 ? QString() : column.value("label").toString();
            QCOMPARE(table->horizontalHeaderItem(col)->text(), header);
            if (col == 0 || col == 1) {
                auto* spin = qobject_cast<QDoubleSpinBox*>(cell);
                QVERIFY(spin != nullptr);
                QCOMPARE(spin->minimum(), column.value("min").toDouble());
                QCOMPARE(spin->maximum(), column.value("max").toDouble());
                QCOMPARE(spin->singleStep(), column.value("step").toDouble());
            } else if (col == 2) {
                QVERIFY(qobject_cast<QCheckBox*>(cell) != nullptr);
            } else {
                auto* button = qobject_cast<QAbstractButton*>(cell);
                QVERIFY(button != nullptr);
                QCOMPARE(button->text(), column.value("label").toString());
            }
        }
    } else if (kind == "text") {
        QVERIFY2(qobject_cast<QLineEdit*>(object) != nullptr, qPrintable(id));
    } else if (kind == "colour") {
        auto* swatch = qobject_cast<ColorSwatchButton*>(object);
        QVERIFY2(swatch != nullptr, qPrintable(id));
        QCOMPARE(ColorSwatchButton::colorToHex(swatch->color()).toUpper(),
                 control.value("default").toString());
    }
    if (auto* widget = qobject_cast<QWidget*>(object)) {
        QCOMPARE(widget->toolTip(), control.value("tooltip").toString());
        if (control.value("availability").toObject().value("enabled") == QJsonValue(false)) {
            QVERIFY(!widget->isEnabled());
        }
    }
    const QString key = control.value("binding").toObject().value("setting").toString();
    if (!key.isEmpty()) {
        QCOMPARE(classifySettingsKey(key), SettingsScope::Station);
    }
}
}

class SetupDescriptionParityTest : public QObject {
    Q_OBJECT
private slots:
    void describedDisplayScalarsMatchDesktopAndStayBoardIndependent()
    {
        RadioModel model;
        SpectrumDefaultsPage spectrum(&model);
        SpectrumPeaksPage peaks(&model);
        WaterfallDefaultsPage waterfall(&model);
        MultimeterPage multimeter(&model);
        TxDisplayPage tx(&model);
        SetupDescriptionService service;
        const QJsonObject display = projectedCategory(service.display(), 11);
        const QJsonArray pages = display.value("pages").toArray();
        QCOMPARE(pages.size(), 5);
        const QStringList expectedIds{
            "display.spectrumDefaults.fftSize", "display.spectrumDefaults.window",
            "display.spectrumDefaults.hzPerBinTarget", "display.spectrumDefaults.fps",
            "display.spectrumDefaults.detector", "display.spectrumDefaults.averaging",
            "display.spectrumDefaults.averageTime", "display.spectrumDefaults.decimation",
            "display.spectrumDefaults.panFill", "display.spectrumDefaults.fillAlpha",
            "display.spectrumDefaults.gradient", "display.spectrumDefaults.peakHold",
            "display.spectrumDefaults.peakDelay",
            "display.spectrumPeaks.activePeakHold", "display.spectrumPeaks.activePeakHoldTime",
            "display.spectrumPeaks.activePeakHoldDropRate",
            "display.spectrumPeaks.activePeakHoldFill", "display.spectrumPeaks.activePeakHoldOnTx",
            "display.spectrumPeaks.activePeakHoldColor",
            "display.spectrumPeaks.peakBlobs", "display.spectrumPeaks.peakBlobCount",
            "display.spectrumPeaks.peakBlobInsideFilter", "display.spectrumPeaks.peakBlobHold",
            "display.spectrumPeaks.peakBlobHoldTime", "display.spectrumPeaks.peakBlobHoldDrop",
            "display.spectrumPeaks.peakBlobFallRate", "display.spectrumPeaks.peakBlobColor",
            "display.spectrumPeaks.peakBlobTextColor",
            "display.waterfallDefaults.detector", "display.waterfallDefaults.averaging",
            "display.waterfallDefaults.averageTime", "display.waterfallDefaults.updatePeriod",
            "display.waterfallDefaults.stopOnTx", "display.waterfallDefaults.opacity",
            "display.waterfallDefaults.showRxFilter", "display.waterfallDefaults.showTxFilter",
            "display.waterfallDefaults.showRxZeroLine", "display.waterfallDefaults.showTxZeroLine",
            "display.multimeter.pollingDelay", "display.txDisplay.fftSize",
            "display.txDisplay.window", "display.txDisplay.panDetector",
            "display.txDisplay.panAveraging", "display.txDisplay.panAvTime",
            "display.txDisplay.panNormalize", "display.txDisplay.wfDetector",
            "display.txDisplay.wfAveraging", "display.txDisplay.wfAvTime"};
        const QList<QJsonValue> expectedDefaults{
            ControlRanges::kDisplayFftSizeDefault, ControlRanges::kDisplayFftWindowDefault,
            ControlRanges::kDisplayHzPerBinTargetDefault, ControlRanges::kDisplaySpectrumFpsDefault,
            ControlRanges::kDisplaySpectrumDetectorDefault,
            ControlRanges::kDisplaySpectrumAveragingDefault,
            ControlRanges::kDisplaySpectrumAvgTimeDefaultMs,
            ControlRanges::kDisplayDecimationDefault,
            true, 70, false, false, 2000,
            false, 2000, 6, false, false, "#FFD700FF", false, 3, false, false, 500, false, 6,
            "#FF4500FF", "#7FFF00FF",
            ControlRanges::kDisplayWaterfallDetectorDefault,
            ControlRanges::kDisplayWaterfallAveragingDefault,
            ControlRanges::kDisplayWaterfallAvgTimeDefaultMs,
            30, false, 100,
            false, true, false, false,
            100, TxAnalyzer::kDefaultFftSize, TxAnalyzer::kDefaultWindowType,
            TxAnalyzer::kDefaultPanDetector, TxAnalyzer::kDefaultPanAveraging,
            TxAnalyzer::kDefaultPanAvTimeMs, TxAnalyzer::kDefaultPanNormalize,
            TxAnalyzer::kDefaultWfDetector, TxAnalyzer::kDefaultWfAveraging,
            TxAnalyzer::kDefaultWfAvTimeMs};
        QStringList actualIds;
        QCOMPARE(pages.at(0).toObject().value("sections").toArray().at(0).toObject().value("title"),
                 QJsonValue("Fast Fourier Transform"));
        QCOMPARE(pages.at(0).toObject().value("sections").toArray().at(1).toObject().value("title"),
                 QJsonValue("Rendering"));
        QCOMPARE(pages.at(1).toObject().value("sections").toArray().at(0).toObject().value("title"),
                 QJsonValue("Active Peak Hold"));
        QCOMPARE(pages.at(1).toObject().value("sections").toArray().at(1)
                     .toObject().value("title"), QJsonValue("Peak Blobs"));
        QCOMPARE(pages.at(1).toObject().value("where"), QJsonValue("phone"));
        QCOMPARE(pages.at(2).toObject().value("sections").toArray().at(0).toObject().value("title"),
                 QJsonValue("Display"));
        QCOMPARE(pages.at(2).toObject().value("sections").toArray().at(1)
                     .toObject().value("title"), QJsonValue("Overlays"));
        QCOMPARE(pages.at(2).toObject().value("where"), QJsonValue("phone"));
        QCOMPARE(pages.at(3).toObject().value("sections").toArray().at(0).toObject().value("title"),
                 QJsonValue("Multimeter"));
        QCOMPARE(pages.at(4).toObject().value("sections").toArray().at(1).toObject().value("title"),
                 QJsonValue("Panadapter"));
        QCOMPARE(pages.at(4).toObject().value("sections").toArray().at(2).toObject().value("title"),
                 QJsonValue("Waterfall"));
        // The native group titles are the described section titles.
        const QList<QGroupBox*> peakGroups = peaks.findChildren<QGroupBox*>();
        QStringList peakTitles;
        for (const QGroupBox* group : peakGroups) { peakTitles << group->title(); }
        QCOMPARE(peakTitles, (QStringList{"Active Peak Hold", "Peak Blobs"}));
        // Every one of the page's fifteen rows is described.
        int taggedPeakRows = 0;
        for (QObject* object : peaks.findChildren<QObject*>()) {
            taggedPeakRows += object->property("nereusSetupId").isValid() ? 1 : 0;
        }
        QCOMPARE(taggedPeakRows, 15);
        for (int p = 0; p < pages.size(); ++p) {
            QWidget* native = p == 0 ? static_cast<QWidget*>(&spectrum)
                : p == 1 ? static_cast<QWidget*>(&peaks)
                : p == 2 ? static_cast<QWidget*>(&waterfall)
                : p == 3 ? static_cast<QWidget*>(&multimeter) : static_cast<QWidget*>(&tx);
            for (const QJsonValue& section : pages.at(p).toObject().value("sections").toArray()) {
                for (const QJsonValue& raw : section.toObject().value("controls").toArray()) {
                    const QJsonObject control = raw.toObject();
                    QCOMPARE(control.value("default"), expectedDefaults.at(actualIds.size()));
                    actualIds << control.value("id").toString();
                    QVERIFY(SetupDescriptionService::validateDisplaySettingBinding(control)
                            || SetupDescriptionService::validateDisplayPhoneBinding(control));
                    compareControl(*native, control);
                    if (control.value("binding").toObject().contains("phone")
                        && control.value("kind") != QJsonValue("toggle")) {
                        auto* const widget = qobject_cast<QWidget*>(
                            bySetupId(*native, control.value("id").toString()));
                        QVERIFY(widget != nullptr);
                        QWidget* field = widget;
                        QFormLayout* form = nullptr;
                        for (QWidget* parent = widget->parentWidget(); parent && !form;
                             parent = parent->parentWidget()) {
                            auto* candidate = qobject_cast<QFormLayout*>(parent->layout());
                            if (candidate && candidate->labelForField(field)) { form = candidate; }
                            else { field = parent; }
                        }
                        QVERIFY(form != nullptr);
                        auto* const label = qobject_cast<QLabel*>(form->labelForField(field));
                        QVERIFY(label != nullptr);
                        QCOMPARE(label->text(), control.value("label").toString());
                    }
                }
            }
        }
        QCOMPARE(actualIds, expectedIds);
        model.setBoardForTest(HPSDRHW::HermesLite);
        service.setRadioContext(model.boardCapabilities(), model.hardwareProfile().model);
        QCOMPARE(projectedCategory(service.display(), 11), display);
    }
    void describedV12DisplayRowsMatchNativePages()
    {
        RadioModel model;
        SpectrumDefaultsPage spectrum(&model);
        WaterfallDefaultsPage waterfall(&model);
        GridScalesPage grid(&model);
        MultimeterPage multimeter(&model);
        TxDisplayPage tx(&model);
        Display3DSetupPage threeD(nullptr);
        ColorsThemePage colours(&model);
        SetupDescriptionService service;
        const QHash<QString, QWidget*> nativeByPage{
            {"display.spectrumDefaults", &spectrum}, {"display.waterfallDefaults", &waterfall},
            {"display.gridScales", &grid}, {"display.multimeter", &multimeter},
            {"display.txDisplay", &tx}, {"display.threeD", &threeD},
            {"appearance.colorsTheme", &colours}};
        int described = 0;
        QHash<QWidget*, int> taggedV12;
        for (const QJsonObject& category : {service.category(QStringLiteral("display")),
                                            service.category(QStringLiteral("appearance"))}) {
            for (const QJsonValue& rawPage : category.value("pages").toArray()) {
                const QJsonObject page = rawPage.toObject();
                for (const QJsonValue& rawSection : page.value("sections").toArray()) {
                    const QJsonObject section = rawSection.toObject();
                    for (const QJsonValue& raw : section.value("controls").toArray()) {
                        const QJsonObject control = raw.toObject();
                        if (control.value("requiresDescriptionVersion") != QJsonValue(12)) {
                            continue;
                        }
                        const QString id = control.value("id").toString();
                        QWidget* native = nativeByPage.value(page.value("id").toString());
                        QVERIFY2(native != nullptr, qPrintable(id));
                        ++described;
                        ++taggedV12[native];
                        compareControl(*native, control);
                        auto* widget = qobject_cast<QWidget*>(bySetupId(*native, id));
                        QVERIFY2(widget != nullptr, qPrintable(id));
                        // A row inside a native group sits in the section of
                        // that title.
                        for (QWidget* parent = widget->parentWidget(); parent;
                             parent = parent->parentWidget()) {
                            if (auto* group = qobject_cast<QGroupBox*>(parent)) {
                                QCOMPARE(group->title(), section.value("title").toString());
                                break;
                            }
                        }
                        const QString kind = control.value("kind").toString();
                        if (kind == QLatin1String("toggle") || kind == QLatin1String("button")) {
                            continue;
                        }
                        const QString label = nativeRowLabel(*native, widget);
                        if (control.contains("perBand")) {
                            // The desktop names the band being edited.
                            PanadapterModel* pan = model.panadapters().isEmpty()
                                ? nullptr : model.panadapters().first();
                            const QString perBand = control.value("perBand").toObject()
                                .value("label").toString();
                            QVERIFY2(label == control.value("label").toString()
                                         || (pan && label == perBand.arg(bandLabel(pan->band()))),
                                     qPrintable(id + QStringLiteral(": ") + label));
                        } else {
                            QCOMPARE(label, control.value("label").toString());
                        }
                    }
                }
            }
        }
        QCOMPARE(described, 53);
        // The Spectrum Defaults section of the two top actions and the
        // Appearance reset carry no native group; the rest match groups.
        QCOMPARE(taggedV12.value(&grid), 12);
        QCOMPARE(taggedV12.value(&threeD), 7);
        // Every tagged widget on the two new pages is described.
        for (QWidget* page : {static_cast<QWidget*>(&grid), static_cast<QWidget*>(&threeD)}) {
            int tagged = 0;
            for (QObject* object : page->findChildren<QObject*>()) {
                tagged += object->property("nereusSetupId").isValid() ? 1 : 0;
            }
            QCOMPARE(tagged, taggedV12.value(page));
        }
        // Rows built but not described carry no id.
        for (QObject* object : spectrum.findChildren<QObject*>()) {
            const QString id = object->property("nereusSetupId").toString();
            QVERIFY(!id.contains(QLatin1String("lineWidth"), Qt::CaseSensitive)
                    || id == QLatin1String("display.spectrumDefaults.noiseFloorLineWidth"));
            QVERIFY(!id.contains(QLatin1String("calOffset")));
            QVERIFY(!id.contains(QLatin1String("threadPriority")));
        }
    }

    // Normalize applies to the Average, Sample and RMS detectors only
    // (Thetis specHPSDR.cs updateNormalizePan); the page follows the detector.
    void normalizeFollowsTheSpectrumDetector()
    {
        SpectrumWidget widget;
        widget.setDispNormalize(true);
        widget.setSpectrumDetector(SpectrumDetector::Peak);
        QVERIFY(widget.dispNormalize());
        QVERIFY(!widget.normalizeActive());
        widget.setSpectrumDetector(SpectrumDetector::Rosenfell);
        QVERIFY(!widget.normalizeActive());
        for (const SpectrumDetector detector :
             {SpectrumDetector::Average, SpectrumDetector::Sample, SpectrumDetector::RMS}) {
            widget.setSpectrumDetector(detector);
            QVERIFY(widget.normalizeActive());
        }
        widget.setDispNormalize(false);
        QVERIFY(!widget.normalizeActive());
        QVERIFY(!normalizeAppliesToDetector(0));
        QVERIFY(!normalizeAppliesToDetector(1));
        QVERIFY(normalizeAppliesToDetector(2));
        QVERIFY(normalizeAppliesToDetector(3));
        QVERIFY(normalizeAppliesToDetector(4));
        QVERIFY(!normalizeAppliesToDetector(5));
    }

    // Reset to Smooth Defaults sets the log-recursive averaging the
    // renderer reads (the legacy mode it set used to reach nothing).
    void smoothDefaultsSetLogRecursiveAveraging()
    {
        RadioModel model;
        SpectrumWidget widget;
        model.setSpectrumSink(&widget);
        widget.setSpectrumAveraging(SpectrumAveraging::None);
        widget.setPanFillEnabled(true);
        model.applyClaritySmoothDefaults();
        QCOMPARE(widget.spectrumAveraging(), SpectrumAveraging::LogRecursive);
        QCOMPARE(widget.wfColorScheme(), WfColorScheme::ClarityBlue);
        QVERIFY(!widget.panFillEnabled());
        QVERIFY(widget.wfAgcEnabled());
        QCOMPARE(widget.wfUpdatePeriodMs(), 30);
    }

    void describedAppearanceColoursMatchNativePage()
    {
        RadioModel model;
        ColorsThemePage page(&model);
        SetupDescriptionService service;
        const QJsonObject appearance = projectedCategory(service.appearance(), 11);
        QCOMPARE(appearance.value("category").toObject().value("where"), QJsonValue("phone"));
        QCOMPARE(appearance.value("category").toObject().value("coverage"), QJsonValue("partial"));
        const QJsonArray pages = appearance.value("pages").toArray();
        QCOMPARE(pages.size(), 2);
        QCOMPARE(pages.first().toObject().value("title"), QJsonValue("Colors & Theme"));
        QCOMPARE(pages.first().toObject().value("where"), QJsonValue("phone"));
        QCOMPARE(pages.first().toObject().value("coverage"), QJsonValue("partial"));
        const QJsonArray sections = pages.first().toObject().value("sections").toArray();
        QCOMPARE(sections.size(), 1);
        QCOMPARE(sections.first().toObject().value("title"), QJsonValue("Spectrum"));
        const QJsonArray described = sections.first().toObject().value("controls").toArray();
        QCOMPARE(described.size(), 10);
        const QStringList expectedKeys{
            "DisplayFillColor", "DisplayGridColor", "DisplayGridFineColor",
            "DisplayHGridColor", "DisplayGridTextColor", "DisplayBandEdgeColor",
            "DisplayRxZeroLineColor", "DisplayTxZeroLineColor", "DisplayRxFilterColor",
            "DisplayTxFilterColor"};
        QStringList actualKeys;
        for (const QJsonValue& raw : described) {
            const QJsonObject control = raw.toObject();
            actualKeys << control.value("binding").toObject().value("phone").toString();
            QVERIFY(SetupDescriptionService::validateAppearanceColourBinding(control));
            compareControl(page, control);
            auto* swatch = qobject_cast<ColorSwatchButton*>(bySetupId(page,
                control.value("id").toString()));
            QVERIFY(swatch != nullptr);
            auto* form = qobject_cast<QFormLayout*>(swatch->parentWidget()->layout());
            QVERIFY(form != nullptr);
            auto* label = qobject_cast<QLabel*>(form->labelForField(swatch));
            QVERIFY(label != nullptr);
            QCOMPARE(label->text(), control.value("label").toString());
        }
        QCOMPARE(actualKeys, expectedKeys);
        QVERIFY(bySetupId(page, QStringLiteral("appearance.colorsTheme.waterfallLowColor")) == nullptr);
        model.setBoardForTest(HPSDRHW::HermesLite);
        service.setRadioContext(model.boardCapabilities(), model.hardwareProfile().model);
        QCOMPARE(projectedCategory(service.appearance(), 11), appearance);
    }
    void describedMeterStylesMatchNativePage()
    {
        RadioModel model;
        MeterStylesPage page(&model);
        SetupDescriptionService service;
        const QJsonObject appearance = projectedCategory(service.appearance(), 11);
        const QJsonArray pages = appearance.value("pages").toArray();
        QCOMPARE(pages.size(), 2);
        const QJsonObject meterPage = pages.at(1).toObject();
        QCOMPARE(meterPage.value("id"), QJsonValue("appearance.meterStyles"));
        QCOMPARE(meterPage.value("title"), QJsonValue("Meter Styles"));
        QCOMPARE(meterPage.value("where"), QJsonValue("phone"));
        QCOMPARE(meterPage.value("coverage"), QJsonValue("partial"));
        const QJsonArray sections = meterPage.value("sections").toArray();
        QCOMPARE(sections.size(), 1);
        QCOMPARE(sections.first().toObject().value("title"), QJsonValue("S-Meter"));
        const QJsonArray described = sections.first().toObject().value("controls").toArray();
        QCOMPARE(described.size(), 3);
        const QStringList ids{QStringLiteral("appearance.meterStyles.face"),
                              QStringLiteral("appearance.meterStyles.peakHold"),
                              QStringLiteral("appearance.meterStyles.peakDecay")};
        const QStringList keys{QStringLiteral("SMeter_FaceStyle"),
                               QStringLiteral("PeakHoldEnabled"),
                               QStringLiteral("PeakDecayRate")};
        const QStringList decayNames{QStringLiteral("Fast"), QStringLiteral("Medium"),
                                     QStringLiteral("Slow")};
        const QList<QJsonValue> defaults{0, true, 1};
        auto* face = qobject_cast<QComboBox*>(bySetupId(page, ids.at(0)));
        auto* hold = qobject_cast<QCheckBox*>(bySetupId(page, ids.at(1)));
        auto* decay = qobject_cast<QComboBox*>(bySetupId(page, ids.at(2)));
        QVERIFY(face != nullptr);
        QVERIFY(hold != nullptr);
        QVERIFY(decay != nullptr);
        const QList<QWidget*> widgets{face, hold, decay};
        auto* group = qobject_cast<QGroupBox*>(face->parentWidget());
        QVERIFY(group != nullptr);
        QCOMPARE(group->title(), QStringLiteral("S-Meter"));
        auto* form = qobject_cast<QFormLayout*>(group->layout());
        QVERIFY(form != nullptr);
        QCOMPARE(form->rowCount(), 3);
        for (int i = 0; i < described.size(); ++i) {
            const QJsonObject control = described.at(i).toObject();
            QCOMPARE(form->itemAt(i, QFormLayout::FieldRole)->widget(), widgets.at(i));
            QCOMPARE(control.value("id"), QJsonValue(ids.at(i)));
            QCOMPARE(control.value("binding").toObject().value("phone"), QJsonValue(keys.at(i)));
            QCOMPARE(control.value("requiresDescriptionVersion"), QJsonValue(7));
            QCOMPARE(control.value("applies"), QJsonValue("live"));
            QCOMPARE(control.value("default"), defaults.at(i));
            QVERIFY(SetupDescriptionService::validateAppearanceMeterStyleBinding(control));
            QCOMPARE(widgets.at(i)->toolTip(), control.value("tooltip").toString());
            if (i != 1) {
                auto* label = qobject_cast<QLabel*>(form->labelForField(widgets.at(i)));
                QVERIFY(label != nullptr);
                QCOMPARE(label->text(), control.value("label").toString());
                auto* combo = qobject_cast<QComboBox*>(widgets.at(i));
                const QJsonArray options = control.value("options").toArray();
                QCOMPARE(combo->count(), options.size());
                for (int option = 0; option < options.size(); ++option) {
                    QCOMPARE(combo->itemText(option), options.at(option).toObject().value("label").toString());
                    QCOMPARE(options.at(option).toObject().value("value"), QJsonValue(option));
                    if (i == 0) {
                        QCOMPARE(combo->itemData(option).toInt(), option);
                        QCOMPARE(combo->itemText(option), SMeterWidget::faceStyleLabel(
                            static_cast<SMeterWidget::FaceStyle>(option)));
                    } else {
                        QCOMPARE(combo->itemData(option).toString(), decayNames.at(option));
                    }
                }
            } else {
                QCOMPARE(hold->text(), control.value("label").toString());
            }
        }
        QCOMPARE(SMeterWidget::faceStyleKey(SMeterWidget::FaceStyle::AgedCream),
                 QStringLiteral("AgedCream"));
        model.setBoardForTest(HPSDRHW::HermesLite);
        service.setRadioContext(model.boardCapabilities(), model.hardwareProfile().model);
        QCOMPARE(projectedCategory(service.appearance(), 11), appearance);
        QVERIFY(bySetupId(page, QStringLiteral("appearance.meterStyles.smallFilter")) == nullptr);
    }
    void describedCatNetworkRowsMatchRemoteDesktop()
    {
        // The phone is a remote client: its rows are the remote window's
        // (its captions, the Core's verbs). Tooltips there are the Core's
        // availability reasons, so they are not compared here.
        RadioModel model(RadioModel::Role::Remote);
        FourO3APage fourO3A(&model);
        RfKitPage rfKit(&model);
        SetupDescriptionService service;
        int described = 0;
        for (const QJsonValue& raw : controls(service.category(QStringLiteral("catNetwork")))) {
            const QJsonObject control = raw.toObject();
            const QString id = control.value("id").toString();
            if (control.value("requiresDescriptionVersion") != QJsonValue(15)) { continue; }
            ++described;
            QWidget& page = id.startsWith("catNetwork.rfKit.") ? static_cast<QWidget&>(rfKit)
                                                               : static_cast<QWidget&>(fourO3A);
            QObject* object = bySetupId(page, id);
            QVERIFY2(object != nullptr, qPrintable(id + " has no desktop widget"));
            const QString kind = control.value("kind").toString();
            if ((kind == "toggle" || kind == "button")
                && object->property("nereusSetupId").toString() == id) {
                auto* button = qobject_cast<QAbstractButton*>(object);
                QVERIFY2(button != nullptr, qPrintable(id));
                QCOMPARE(button->text(), control.value("label").toString());
            }
            if (kind == "integer") {
                auto* spin = qobject_cast<QSpinBox*>(object);
                QVERIFY2(spin != nullptr, qPrintable(id));
                QCOMPARE(spin->minimum(), control.value("min").toInt());
                QCOMPARE(spin->maximum(), control.value("max").toInt());
            }
            if (kind == "choice" && control.contains("options")) {
                auto* combo = qobject_cast<QComboBox*>(object);
                QVERIFY2(combo != nullptr, qPrintable(id));
                QCOMPARE(combo->count(), control.value("options").toArray().size());
            }
        }
        QCOMPARE(described, 80);
    }

    void describedDiagnosticsReadoutsMatchDesktop()
    {
        RadioModel model;
        RadioStatusPage status(&model);
        ConnectionQualityPage quality(&model);
        SetupDescriptionService service;
        const QJsonObject diagnostics = service.category(QStringLiteral("diagnostics"));
        int described = 0;
        for (const QJsonValue& raw : controls(diagnostics)) {
            const QJsonObject control = raw.toObject();
            const QString id = control.value("id").toString();
            if (id.startsWith("diagnostics.settingsValidation.")) { continue; }
            ++described;
            compareControl(id.startsWith("diagnostics.radioStatus.")
                               ? static_cast<QWidget&>(status) : static_cast<QWidget&>(quality),
                           control);
        }
        QCOMPARE(described, 18);
    }

    void settingsValidationPanelMatchesDesktopActions()
    {
        RadioModel model;
        SettingsValidationPage page(&model);
        SetupDescriptionService service;
        const QJsonObject diagnostics = service.category(QStringLiteral("diagnostics"));
        const QJsonObject panel = diagnostics.value("pages").toArray().last().toObject()
            .value("sections").toArray().first().toObject()
            .value("controls").toArray().first().toObject();
        QVERIFY(SetupDescriptionService::validateSettingsHygienePanel(panel));
        const QJsonArray actions = panel.value("actions").toArray();
        QCOMPARE(actions.size(), 3);
        const auto buttons = page.findChildren<QPushButton*>();
        QCOMPARE(buttons.size(), 3);
        for (int i = 0; i < actions.size(); ++i) {
            QCOMPARE(buttons.at(i)->text(), actions.at(i).toObject().value("label").toString());
        }
        QCOMPARE(actions.at(2).toObject().value("confirmation").toObject()
                     .value("message"), QJsonValue("Forget all settings for this radio?"));
        // G-38: Repair is enabled by its own gate, like Forget's rules.
        QCOMPARE(actions.at(1).toObject().value("gate").toObject().value("min"), QJsonValue(2));
        QCOMPARE(actions.at(1).toObject().value("paired"), QJsonValue(true));
        QCOMPARE(actions.at(1).toObject().value("offAir"), QJsonValue(true));
    }

    void describedPaBypassMatchesG2eDesktopOnly()
    {
        RadioModel g2e;
        g2e.setHpsdrModelForTest(HPSDRModel::ANAN_G2E);
        PaGainByBandPage page(&g2e);
        page.applyCapabilityVisibility(g2e.boardCapabilities());
        SetupDescriptionService service;
        service.setRadioContext(g2e.boardCapabilities(), g2e.hardwareProfile().model);
        // Version 13 added the Watt Meter page; this reads version 12's.
        const QJsonArray pages = projectedCategory(service.pa(), 12).value("pages").toArray();
        QCOMPARE(pages.size(), 2);
        const QJsonObject gain = pages.first().toObject();
        QCOMPARE(gain.value("id"), QJsonValue("pa.gain"));
        QCOMPARE(gain.value("coverage"), QJsonValue("partial"));
        const QJsonArray described = gain.value("sections").toArray().first().toObject()
            .value("controls").toArray();
        QCOMPARE(described.size(), 1);
        const QJsonObject control = described.first().toObject();
        auto* check = page.bypassPaSettingsCheckForTest();
        QVERIFY(check != nullptr);
        QCOMPARE(check->property("nereusSetupId").toString(), control.value("id").toString());
        QCOMPARE(check->text(), control.value("label").toString());
        QCOMPARE(check->toolTip(), control.value("tooltip").toString());
        QVERIFY(!check->isHidden());
        QVERIFY(SetupDescriptionService::validatePaBypassBinding(control));
        QVERIFY(!control.value("gate").toObject().contains("offAir"));
        QVERIFY(!control.value("gate").toObject().contains("transmit"));

        RadioModel other;
        other.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        PaGainByBandPage otherPage(&other);
        otherPage.applyCapabilityVisibility(other.boardCapabilities());
        service.setRadioContext(other.boardCapabilities(), other.hardwareProfile().model);
        QCOMPARE(projectedCategory(service.pa(), 12).value("pages").toArray().size(), 1);
        QVERIFY(otherPage.bypassPaSettingsCheckForTest()->isHidden());
    }

    void describedPaReadoutsMatchDesktopRows()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Saturn);
        PaValuesPage page(&model);
        SetupDescriptionService service;
        service.setRadioContext(model.boardCapabilities(), model.hardwareProfile().model);
        // Version 12's rows; describedV13PaRowsMatchNativePages has 13's.
        const QJsonObject pa = projectedCategory(service.pa(), 12);
        QVERIFY(!pa.isEmpty());
        const QJsonArray sections = pa.value("pages").toArray().first().toObject()
            .value("sections").toArray();
        QCOMPARE(sections.size(), 3);
        const QStringList expectedIds{QStringLiteral("pa.values.forwardCalibrated"),
                                      QStringLiteral("pa.values.forwardRawPower"),
                                      QStringLiteral("pa.values.reflectedPower"),
                                      QStringLiteral("pa.values.swr"),
                                      QStringLiteral("pa.values.drive"),
                                      QStringLiteral("pa.values.paCurrent"),
                                      QStringLiteral("pa.values.dcVoltage"),
                                      QStringLiteral("pa.values.forwardVoltage"),
                                      QStringLiteral("pa.values.reflectedVoltage"),
                                      QStringLiteral("pa.values.forwardAdc"),
                                      QStringLiteral("pa.values.reflectedAdc")};
        QStringList actualIds;
        for (const QJsonValue& rawSection : sections) {
            const QJsonObject section = rawSection.toObject();
            const QJsonArray described = section.value("controls").toArray();
            for (const QJsonValue& rawControl : described) {
                const QJsonObject control = rawControl.toObject();
                const QString id = control.value("id").toString();
                actualIds.append(id);
                auto* widget = qobject_cast<MetricLabel*>(bySetupId(page, id));
                QVERIFY2(widget != nullptr, qPrintable(id));
                QVERIFY(id == QLatin1String("pa.values.drive")
                            ? SetupDescriptionService::validatePaDriveReadoutBinding(control)
                            : (id == QLatin1String("pa.values.paCurrent")
                               || id == QLatin1String("pa.values.dcVoltage"))
                                ? SetupDescriptionService::validatePaTelemetryReadoutBinding(control)
                                : SetupDescriptionService::validatePaReadoutBinding(control));
                QCOMPARE(widget->toolTip(), control.value("tooltip").toString());
                auto* group = qobject_cast<QGroupBox*>(widget->parentWidget());
                QVERIFY(group != nullptr);
                QCOMPARE(group->title(), section.value("title").toString());
                auto* form = qobject_cast<QFormLayout*>(group->layout());
                QVERIFY(form != nullptr);
                auto* label = qobject_cast<QLabel*>(form->labelForField(widget));
                QVERIFY(label != nullptr);
                QCOMPARE(label->text(), control.value("label").toString());
            }
        }
        QCOMPARE(actualIds, expectedIds);
        const QJsonArray power = sections.first().toObject().value("controls").toArray();
        const QJsonArray raw = sections.last().toObject().value("controls").toArray();
        QCOMPARE(power.size(), 5);
        QCOMPARE(raw.size(), 2);
        model.transmitModel().setPower(37);
        QCOMPARE(page.driveTextForTest(), QStringLiteral("37 W"));
    }

    // Version 13 (R-R3-49, R-IOS-18): the PA rows match the Watt Meter and
    // PA Values pages widget for widget.
    void describedV13PaRowsMatchNativePages()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        // As the Core seeds a radio's table on connect (RadioModel), so the
        // page builds the board class's ten points.
        model.calibrationControllerMutable().setPaCalProfile(
            PaCalProfile::defaults(paCalBoardClassFor(HPSDRModel::ANAN_G2)));
        PaWattMeterPage watt(&model);
        watt.applyCapabilityVisibility(model.boardCapabilities());
        PaValuesPage values(&model);
        values.applyCapabilityVisibility(model.boardCapabilities());
        SetupDescriptionService service;
        service.setRadioContext(model.boardCapabilities(), model.hardwareProfile().model);
        const QJsonObject pa = projectedCategory(service.pa(), 13);
        int compared = 0;
        for (const QJsonValue& rawPage : pa.value("pages").toArray()) {
            const QJsonObject page = rawPage.toObject();
            QWidget& nativePage = page.value("id") == QJsonValue("pa.wattMeter")
                ? static_cast<QWidget&>(watt) : static_cast<QWidget&>(values);
            for (const QJsonValue& rawSection : page.value("sections").toArray()) {
                const QJsonObject section = rawSection.toObject();
                for (const QJsonValue& raw : section.value("controls").toArray()) {
                    const QJsonObject control = raw.toObject();
                    if (control.value("requiresDescriptionVersion") != QJsonValue(13)) {
                        continue;
                    }
                    const QString id = control.value("id").toString();
                    auto* widget = qobject_cast<QWidget*>(bySetupId(nativePage, id));
                    QVERIFY2(widget != nullptr, qPrintable(id + " has no desktop widget"));
                    QCOMPARE(widget->toolTip(), control.value("tooltip").toString());
                    const QString kind = control.value("kind").toString();
                    if (kind == "button" || kind == "toggle") {
                        auto* button = qobject_cast<QAbstractButton*>(widget);
                        QVERIFY2(button != nullptr, qPrintable(id));
                        QCOMPARE(button->text(), control.value("label").toString());
                        // The page's own controls sit outside any group.
                        QVERIFY(qobject_cast<QGroupBox*>(widget->parentWidget()) == nullptr);
                        if (kind == "toggle") {
                            QCOMPARE(button->isChecked(), control.value("default").toBool());
                        }
                    } else {
                        auto* group = qobject_cast<QGroupBox*>(widget->parentWidget());
                        QVERIFY2(group != nullptr, qPrintable(id));
                        QCOMPARE(group->title(), section.value("title").toString());
                        auto* form = qobject_cast<QFormLayout*>(group->layout());
                        QVERIFY(form != nullptr);
                        auto* label = qobject_cast<QLabel*>(form->labelForField(widget));
                        QVERIFY2(label != nullptr, qPrintable(id));
                        QCOMPARE(label->text(), control.value("label").toString());
                        if (kind == "decimal") {
                            auto* spin = qobject_cast<QDoubleSpinBox*>(widget);
                            QVERIFY2(spin != nullptr, qPrintable(id));
                            QCOMPARE(spin->minimum(), control.value("min").toDouble());
                            QCOMPARE(spin->maximum(), control.value("max").toDouble());
                            QCOMPARE(spin->singleStep(), control.value("step").toDouble());
                            QCOMPARE(spin->decimals(), control.value("decimals").toInt());
                            QCOMPARE(spin->value(), control.value("default").toDouble());
                            QCOMPARE(spin->suffix(), QStringLiteral(" ") + control.value("unit").toString());
                        } else {
                            QVERIFY2(qobject_cast<MetricLabel*>(widget) != nullptr, qPrintable(id));
                        }
                    }
                    ++compared;
                }
            }
        }
        QCOMPARE(compared, 15);
    }

    // Version 14 (R-R3-49, R-IOS-18): PA Gain's profile rows match the page.
    void describedV14PaGainRowsMatchNativePage()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        PaGainByBandPage page(&model);
        page.applyCapabilityVisibility(model.boardCapabilities());
        SetupDescriptionService service;
        service.setRadioContext(model.boardCapabilities(), model.hardwareProfile().model);
        QJsonObject gain;
        for (const QJsonValue& p : projectedCategory(service.pa(), 14).value("pages").toArray()) {
            if (p.toObject().value("id") == QJsonValue("pa.gain")) { gain = p.toObject(); }
        }
        QVERIFY(!gain.isEmpty());
        const QJsonArray rows = controls(QJsonObject{{"pages", QJsonArray{gain}}});
        QCOMPARE(rows.size(), 6);
        auto* combo = qobject_cast<QComboBox*>(bySetupId(page, "pa.gain.profile"));
        QVERIFY(combo == page.profileComboForTest());
        QCOMPARE(combo->accessibleName(), rows.at(0).toObject().value("label").toString());
        QCOMPARE(combo->toolTip(), rows.at(0).toObject().value("tooltip").toString());
        for (int i = 1; i <= 4; ++i) {
            const QJsonObject row = rows.at(i).toObject();
            auto* button = qobject_cast<QPushButton*>(bySetupId(page, row.value("id").toString()));
            QVERIFY2(button != nullptr, qPrintable(row.value("id").toString()));
            QCOMPARE(button->text(), row.value("label").toString());
            QCOMPARE(button->toolTip(), row.value("tooltip").toString());
        }
        QVERIFY(page.newButtonForTest() == bySetupId(page, "pa.gain.new"));
        const QJsonObject table = rows.at(5).toObject();
        auto* group = qobject_cast<QGroupBox*>(bySetupId(page, "pa.gain.table"));
        QVERIFY(group != nullptr);
        QCOMPARE(group->title(), table.value("label").toString());
        // The grid's header row, then one row per band.
        auto* grid = qobject_cast<QGridLayout*>(group->layout());
        QVERIFY(grid != nullptr);
        const QJsonArray columns = table.value("columns").toArray();
        for (int c = 0; c < columns.size(); ++c) {
            auto* header = qobject_cast<QLabel*>(grid->itemAtPosition(0, c + 1)->widget());
            QVERIFY(header != nullptr);
            QCOMPARE(header->text(), columns.at(c).toObject().value("label").toString());
        }
        const QJsonArray bands = table.value("rows").toArray();
        for (int b = 0; b < bands.size(); ++b) {
            const Band band = static_cast<Band>(bands.at(b).toObject().value("band").toInt());
            const QString label = bands.at(b).toObject().value("label").toString();
            QCOMPARE(label, bandLabel(band));
            for (int c = 0; c < columns.size(); ++c) {
                const QJsonObject column = columns.at(c).toObject();
                const QString field = column.value("field").toString();
                QWidget* widget = field == "gain" ? static_cast<QWidget*>(page.gainSpinForTest(band))
                    : field == "adjust" ? static_cast<QWidget*>(page.adjustSpinForTest(
                          band, column.value("driveStep").toInt()))
                    : field == "maxPower" ? static_cast<QWidget*>(page.maxPowerSpinForTest(band))
                                          : static_cast<QWidget*>(page.useMaxPowerCheckForTest(band));
                QVERIFY(widget != nullptr);
                QCOMPARE(widget->toolTip(), column.value("tooltip").toString().arg(label));
                if (auto* spin = qobject_cast<QDoubleSpinBox*>(widget)) {
                    QCOMPARE(spin->minimum(), column.value("min").toDouble());
                    QCOMPARE(spin->maximum(), column.value("max").toDouble());
                    QCOMPARE(spin->singleStep(), column.value("step").toDouble());
                    QCOMPARE(spin->decimals(), column.value("decimals").toInt());
                }
            }
        }
    }

    // Version 13: Radio Info, TX Display Cal and the N2ADR switch match
    // their Hardware Config tabs.
    void describedV13HardwareRowsMatchNativeTabs()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::HERMESLITE);
        RadioInfo info;
        info.name = QStringLiteral("Bench HL2");
        info.macAddress = QStringLiteral("00:1C:C0:A2:56:78");
        info.address = QHostAddress(QStringLiteral("10.0.0.123"));
        info.firmwareVersion = 73;
        RadioInfoTab radioInfo(&model);
        radioInfo.populate(info, model.boardCapabilities());
        CalibrationTab calibration(&model);
        Hl2IoBoardTab hl2(&model);
        SetupDescriptionService service;
        service.setRadioContext(model.boardCapabilities(), model.hardwareProfile().model, info);
        const QJsonObject hardware = projectedCategory(service.hardware(), 13);
        int compared = 0;
        for (const QJsonValue& rawPage : hardware.value("pages").toArray()) {
            const QJsonObject page = rawPage.toObject();
            QWidget* tab = page.value("id") == QJsonValue("hardware.radioInfo")
                ? static_cast<QWidget*>(&radioInfo)
                : page.value("id") == QJsonValue("hardware.calibration")
                    ? static_cast<QWidget*>(&calibration) : static_cast<QWidget*>(&hl2);
            for (const QJsonValue& rawSection : page.value("sections").toArray()) {
                const QJsonObject section = rawSection.toObject();
                for (const QJsonValue& raw : section.value("controls").toArray()) {
                    const QJsonObject control = raw.toObject();
                    const QString id = control.value("id").toString();
                    auto* widget = qobject_cast<QWidget*>(bySetupId(*tab, id));
                    QVERIFY2(widget != nullptr, qPrintable(id + " has no desktop widget"));
                    QCOMPARE(widget->toolTip(), control.value("tooltip").toString());
                    const QString kind = control.value("kind").toString();
                    if (kind == "readout") {
                        auto* value = qobject_cast<QLabel*>(widget);
                        QVERIFY2(value != nullptr, qPrintable(id));
                        QCOMPARE(value->text(), control.value("value").toString());
                    }
                    if (kind == "button" || kind == "toggle") {
                        auto* button = qobject_cast<QAbstractButton*>(widget);
                        QVERIFY2(button != nullptr, qPrintable(id));
                        QCOMPARE(button->text(), control.value("label").toString());
                        if (id == QLatin1String("hardware.hl2Io.n2adrFilter")) {
                            auto* group = qobject_cast<QGroupBox*>(widget->parentWidget());
                            QVERIFY(group != nullptr);
                            QCOMPARE(group->title(), section.value("title").toString());
                        }
                        ++compared;
                        continue;
                    }
                    auto* group = qobject_cast<QGroupBox*>(widget->parentWidget());
                    QVERIFY2(group != nullptr, qPrintable(id));
                    QCOMPARE(group->title(), section.value("title").toString());
                    auto* form = qobject_cast<QFormLayout*>(group->layout());
                    QVERIFY(form != nullptr);
                    auto* label = qobject_cast<QLabel*>(form->labelForField(widget));
                    QVERIFY2(label != nullptr, qPrintable(id));
                    QCOMPARE(label->text(), control.value("label").toString());
                    if (kind == "decimal") {
                        auto* spin = qobject_cast<QDoubleSpinBox*>(widget);
                        QVERIFY2(spin != nullptr, qPrintable(id));
                        QCOMPARE(spin->minimum(), control.value("min").toDouble());
                        QCOMPARE(spin->maximum(), control.value("max").toDouble());
                        QCOMPARE(spin->singleStep(), control.value("step").toDouble());
                        QCOMPARE(spin->decimals(), control.value("decimals").toInt());
                        QCOMPARE(spin->value(), control.value("default").toDouble());
                    }
                    if (kind == "choice") {
                        // The sample rate: the same rates, in the same order.
                        auto* combo = qobject_cast<QComboBox*>(widget);
                        QVERIFY2(combo != nullptr, qPrintable(id));
                        const QJsonArray options = control.value("options").toArray();
                        QVERIFY(!options.isEmpty());
                        QCOMPARE(combo->count(), options.size());
                        for (int i = 0; i < options.size(); ++i) {
                            QCOMPARE(combo->itemText(i),
                                     options.at(i).toObject().value("label").toString());
                            QCOMPARE(combo->itemData(i).toInt(),
                                     options.at(i).toObject().value("value").toInt());
                        }
                    }
                    ++compared;
                }
            }
        }
        // Radio Info's seven, its sample rate and copy button, TX Display
        // Cal, N2ADR.
        QCOMPARE(compared, 11);
    }

    // Version 23: Calibration's Rx1 6m LNA row matches the desktop tab's
    // box: its group, label, tooltip, range, step, places and default.
    void describedV23Rx1SixMeterLnaMatchesNativeTab()
    {
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN7000D);
        CalibrationTab calibration(&model);
        SetupDescriptionService service;
        service.setRadioContext(model.boardCapabilities(), model.hardwareProfile().model);
        const QJsonObject hardware = projectedCategory(service.hardware(), 23);
        QJsonObject control;
        QString sectionTitle;
        for (const QJsonValue& page : hardware.value("pages").toArray()) {
            for (const QJsonValue& section : page.toObject().value("sections").toArray()) {
                for (const QJsonValue& raw : section.toObject().value("controls").toArray()) {
                    if (raw.toObject().value("requiresDescriptionVersion") == QJsonValue(23)) {
                        QVERIFY(control.isEmpty());
                        control = raw.toObject();
                        sectionTitle = section.toObject().value("title").toString();
                    }
                }
            }
        }
        QCOMPARE(control.value("id"), QJsonValue("hardware.calibration.rx1_6mLna"));
        auto* spin = qobject_cast<QDoubleSpinBox*>(
            bySetupId(calibration, control.value("id").toString()));
        QVERIFY(spin != nullptr);
        QCOMPARE(spin->objectName(), QStringLiteral("rx1SixMeterLnaSpin"));
        QCOMPARE(spin->toolTip(), control.value("tooltip").toString());
        auto* group = qobject_cast<QGroupBox*>(spin->parentWidget());
        QVERIFY(group != nullptr);
        QCOMPARE(group->title(), sectionTitle);
        auto* form = qobject_cast<QFormLayout*>(group->layout());
        QVERIFY(form != nullptr);
        auto* label = qobject_cast<QLabel*>(form->labelForField(spin));
        QVERIFY(label != nullptr);
        QCOMPARE(label->text(), control.value("label").toString());
        QCOMPARE(spin->minimum(), control.value("min").toDouble());
        QCOMPARE(spin->maximum(), control.value("max").toDouble());
        QCOMPARE(spin->singleStep(), control.value("step").toDouble());
        QCOMPARE(spin->decimals(), control.value("decimals").toInt());
        QCOMPARE(spin->suffix(), QStringLiteral(" ") + control.value("unit").toString());
        QCOMPARE(control.value("default").toDouble(), 13.0);
    }

    // Version 16, with version 18's clock rows: the HL2 Options rows match
    // the desktop's HL2 Options tab: its group, row label, range and unit,
    // default, and whether the box is enabled. A disabled box's tooltip is
    // the row's availability reason. A row enabled only while another row
    // holds a value (enabledWhen) is enabled on the desktop exactly then.
    void describedV18Hl2OptionsMatchNativeTab()
    {
        AppSettings::instance().clear();
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::HERMESLITE);
        Hl2OptionsTab tab(&model);
        SetupDescriptionService service;
        service.setRadioContext(model.boardCapabilities(), model.hardwareProfile().model,
                                RadioInfo{});
        const QJsonObject hardware = projectedCategory(service.hardware(), 18);
        QCOMPARE(hardware.value("version"), QJsonValue(18));
        QJsonObject page;
        for (const QJsonValue& rawPage : hardware.value("pages").toArray()) {
            if (rawPage.toObject().value("id") == QJsonValue("hardware.hl2Io")) {
                page = rawPage.toObject();
            }
        }
        int compared = 0;
        for (const QJsonValue& rawSection : page.value("sections").toArray()) {
            const QJsonObject section = rawSection.toObject();
            if (section.value("title") != QJsonValue("Hermes Lite Options")) { continue; }
            for (const QJsonValue& raw : section.value("controls").toArray()) {
                const QJsonObject control = raw.toObject();
                const QString id = control.value("id").toString();
                auto* widget = qobject_cast<QWidget*>(bySetupId(tab, id));
                QVERIFY2(widget != nullptr, qPrintable(id + " has no desktop widget"));
                auto* group = qobject_cast<QGroupBox*>(widget->parentWidget());
                QVERIFY2(group != nullptr, qPrintable(id));
                QCOMPARE(group->title(), section.value("title").toString());
                const QJsonObject availability = control.value("availability").toObject();
                const QJsonObject enabledWhen = control.value("enabledWhen").toObject();
                if (availability.isEmpty() && !enabledWhen.isEmpty()) {
                    // The row named by the dependency, by its binding.
                    QWidget* source = nullptr;
                    for (const QJsonValue& other : section.value("controls").toArray()) {
                        if (other.toObject().value("binding").toObject().value("radioSetting")
                            == enabledWhen.value("radioSetting")) {
                            source = qobject_cast<QWidget*>(
                                bySetupId(tab, other.toObject().value("id").toString()));
                        }
                    }
                    auto* sourceBox = qobject_cast<QCheckBox*>(source);
                    QVERIFY2(sourceBox != nullptr, qPrintable(id));
                    QCOMPARE(enabledWhen.value("oneOf"), QJsonValue(QJsonArray{true}));
                    QVERIFY2(!sourceBox->isChecked(), qPrintable(id));
                    QVERIFY2(!widget->isEnabled(), qPrintable(id));
                    sourceBox->setChecked(true);
                    QVERIFY2(widget->isEnabled(), qPrintable(id));
                    sourceBox->setChecked(false);
                    QVERIFY2(!widget->isEnabled(), qPrintable(id));
                    QCOMPARE(widget->toolTip(), control.value("tooltip").toString());
                } else if (availability.isEmpty()) {
                    QVERIFY2(widget->isEnabled(), qPrintable(id));
                    QCOMPARE(widget->toolTip(), control.value("tooltip").toString());
                } else {
                    QCOMPARE(availability.value("enabled"), QJsonValue(false));
                    QVERIFY2(!widget->isEnabled(), qPrintable(id));
                    QCOMPARE(widget->toolTip(), availability.value("reason").toString());
                }
                if (control.value("kind") == QJsonValue("toggle")) {
                    auto* box = qobject_cast<QCheckBox*>(widget);
                    QVERIFY2(box != nullptr, qPrintable(id));
                    QCOMPARE(box->text(), control.value("label").toString());
                    QCOMPARE(box->isChecked(), control.value("default").toBool());
                } else {
                    // An integer row is a QSpinBox; a decimal row (the CL2
                    // frequency) is a QDoubleSpinBox with the row's decimals.
                    const bool decimal = control.value("kind") == QJsonValue("decimal");
                    if (!decimal) {
                        QCOMPARE(control.value("kind"), QJsonValue("integer"));
                    }
                    auto* spin = qobject_cast<QAbstractSpinBox*>(widget);
                    QVERIFY2(spin != nullptr, qPrintable(id));
                    // The row label: the grid's label beside the box, or the
                    // box's accessible name where a check box sits there.
                    QString label = spin->accessibleName();
                    if (label.isEmpty()) {
                        auto* grid = qobject_cast<QGridLayout*>(group->layout());
                        QVERIFY(grid != nullptr);
                        int row = -1;
                        int column = -1;
                        int rowSpan = 0;
                        int columnSpan = 0;
                        grid->getItemPosition(grid->indexOf(spin), &row, &column, &rowSpan,
                                              &columnSpan);
                        auto* text = qobject_cast<QLabel*>(
                            grid->itemAtPosition(row, 0)->widget());
                        QVERIFY2(text != nullptr, qPrintable(id));
                        label = text->text();
                    }
                    QCOMPARE(label, control.value("label").toString());
                    const QString suffix = QStringLiteral(" ") + control.value("unit").toString();
                    if (decimal) {
                        auto* box = qobject_cast<QDoubleSpinBox*>(widget);
                        QVERIFY2(box != nullptr, qPrintable(id));
                        QCOMPARE(box->minimum(), control.value("min").toDouble());
                        QCOMPARE(box->maximum(), control.value("max").toDouble());
                        QCOMPARE(box->singleStep(), control.value("step").toDouble());
                        QCOMPARE(box->decimals(), control.value("decimals").toInt());
                        QCOMPARE(box->suffix(), suffix);
                        QCOMPARE(box->value(), control.value("default").toDouble());
                    } else {
                        auto* box = qobject_cast<QSpinBox*>(widget);
                        QVERIFY2(box != nullptr, qPrintable(id));
                        QCOMPARE(box->minimum(), control.value("min").toInt());
                        QCOMPARE(box->maximum(), control.value("max").toInt());
                        QCOMPARE(box->singleStep(), control.value("step").toInt());
                        QCOMPARE(box->suffix(), suffix);
                        QCOMPARE(box->value(), control.value("default").toInt());
                    }
                }
                ++compared;
            }
        }
        QCOMPARE(compared, 9);
    }

    // Version 13 (R-R3-46, R-R3-49): every Alex receive filter row the
    // Core describes is the desktop tab's row: its group, its row label with
    // Bypass / Start / End, its box's range, step, decimals and Thetis
    // default. The Alex-1 tab shows the bank the Core programs for the
    // board (codec::alex::usesBpf1Preselector): BPF1 with the five switches
    // on the ANAN-G2 and the plain Orion MKII, Alex HPF with them on the
    // ANAN-200D (Orion board, no Alex-2).
    // From Thetis console.cs:6827-6837 [v2.10.3.15]:
    //   || (HardwareSpecific.Hardware == HPSDRHW.HermesC10))  //N1GP G2E added (HermesC10) //DK1HLM
    //   { setBPF1ForOrionIISaturn(freq); } else { setAlexHPF(freq); }
    void describedAlexFilterRowsMatchNativeTabs_data()
    {
        QTest::addColumn<int>("model");
        QTest::addColumn<int>("board");
        QTest::addColumn<int>("expected");
        // The Alex-1 tab's five switches, its one shown bank (six rows of
        // three each), and, where the board has Alex-2, Alex-2's bank and
        // master.
        QTest::newRow("ANAN-G2") << int(HPSDRModel::ANAN_G2) << int(HPSDRHW::Saturn)
                                 << 5 + 2 * 6 * 3 + 1;
        QTest::newRow("Orion MKII") << int(HPSDRModel::ORIONMKII) << int(HPSDRHW::OrionMKII)
                                    << 5 + 2 * 6 * 3 + 1;
        QTest::newRow("ANAN-200D") << int(HPSDRModel::ANAN200D) << int(HPSDRHW::Orion)
                                   << 5 + 6 * 3;
    }

    void describedAlexFilterRowsMatchNativeTabs()
    {
        QFETCH(int, model);
        QFETCH(int, board);
        QFETCH(int, expected);
        const auto sku = static_cast<HPSDRModel>(model);
        AppSettings::instance().clear();
        RadioModel radio;
        radio.setHpsdrModelForTest(sku);
        AntennaAlexAlex1Tab alex1(&radio);
        AntennaAlexAlex2Tab alex2(&radio);
        alex1.updateBoardCapabilities(
            codec::alex::usesBpf1Preselector(static_cast<HPSDRHW>(board)));
        SetupDescriptionService service;
        service.setRadioContext(BoardCapsTable::forBoard(static_cast<HPSDRHW>(board)), sku);
        const QJsonObject hardware = projectedCategory(service.hardware(), 13);
        int compared = 0;
        for (const QJsonValue& rawPage : hardware.value("pages").toArray()) {
            const QJsonObject page = rawPage.toObject();
            QWidget* tab = page.value("id") == QJsonValue("hardware.alex1Filters")
                ? static_cast<QWidget*>(&alex1)
                : page.value("id") == QJsonValue("hardware.alex2Filters")
                    ? static_cast<QWidget*>(&alex2) : nullptr;
            if (tab == nullptr) { continue; }
            for (const QJsonValue& rawSection : page.value("sections").toArray()) {
                const QJsonObject section = rawSection.toObject();
                for (const QJsonValue& raw : section.value("controls").toArray()) {
                    const QJsonObject control = raw.toObject();
                    const QString id = control.value("id").toString();
                    auto* widget = qobject_cast<QWidget*>(bySetupId(*tab, id));
                    QVERIFY2(widget != nullptr, qPrintable(id + " has no desktop widget"));
                    QCOMPARE(widget->toolTip(), control.value("tooltip").toString());
                    // The group box the widget sits in has the section's title.
                    QGroupBox* group = nullptr;
                    for (QWidget* up = widget->parentWidget(); up && !group; up = up->parentWidget()) {
                        group = qobject_cast<QGroupBox*>(up);
                    }
                    QVERIFY2(group != nullptr, qPrintable(id));
                    QCOMPARE(group->title(), section.value("title").toString());
                    // A switch above the rows (the Alex-1 tab's five and the
                    // Alex-2 master): its box's text and default.
                    if (id == QLatin1String("hardware.alex2Filters.bypass55MhzBpf")
                        || control.value("binding").toObject().value("radioSetting")
                               .toString().startsWith(QLatin1String("alex/master/"))) {
                        auto* box = qobject_cast<QCheckBox*>(widget);
                        QVERIFY(box != nullptr);
                        QCOMPARE(box->text(), control.value("label").toString());
                        QCOMPARE(box->isChecked(), control.value("default").toBool());
                        // The one the desktop asks about before clearing
                        // carries the desktop's own question.
                        if (id == QLatin1String("hardware.alex1Filters.hpfBypassOnPs")) {
                            QCOMPARE(control.value("confirm").toString(),
                                     AntennaAlexAlex1Tab::imdWarningText());
                            QCOMPARE(control.value("confirmWhen"), QJsonValue(false));
                        } else {
                            QVERIFY2(!control.contains("confirm"), qPrintable(id));
                        }
                        ++compared;
                        continue;
                    }
                    // A row: the form's label for the row, then its column.
                    QFormLayout* form = nullptr;
                    QLabel* rowLabel = nullptr;
                    for (QFormLayout* candidate : tab->findChildren<QFormLayout*>()) {
                        if (auto* label = qobject_cast<QLabel*>(
                                candidate->labelForField(widget->parentWidget()))) {
                            form = candidate;
                            rowLabel = label;
                        }
                    }
                    QVERIFY2(form != nullptr && rowLabel != nullptr, qPrintable(id));
                    const QString column = id.endsWith(QLatin1String(".bypass")) ? "Bypass"
                        : id.endsWith(QLatin1String(".start")) ? "Start" : "End";
                    QCOMPARE(control.value("label").toString(), rowLabel->text() + " " + column);
                    if (column == QLatin1String("Bypass")) {
                        auto* box = qobject_cast<QCheckBox*>(widget);
                        QVERIFY2(box != nullptr, qPrintable(id));
                        QCOMPARE(box->isChecked(), control.value("default").toBool());
                    } else {
                        auto* spin = qobject_cast<QDoubleSpinBox*>(widget);
                        QVERIFY2(spin != nullptr, qPrintable(id));
                        QCOMPARE(spin->minimum(), control.value("min").toDouble());
                        QCOMPARE(spin->maximum(), control.value("max").toDouble());
                        QCOMPARE(spin->singleStep(), control.value("step").toDouble());
                        QCOMPARE(spin->decimals(), control.value("decimals").toInt());
                        QCOMPARE(spin->value(), control.value("default").toDouble());
                        QCOMPARE(spin->suffix(), " " + control.value("unit").toString());
                    }
                    ++compared;
                }
            }
        }
        QCOMPARE(compared, expected);
    }

    void describedHardwareAntennaScalarsMatchDesktop_data()
    {
        QTest::addColumn<int>("model");
        QTest::addColumn<bool>("rxOut");
        QTest::addColumn<bool>("ext1");
        QTest::addColumn<bool>("ext2");
        QTest::addColumn<bool>("overrideRelay");
        QTest::newRow("HPSDR") << int(HPSDRModel::HPSDR) << true << true << true << true;
        QTest::newRow("Hermes") << int(HPSDRModel::HERMES) << true << true << true << false;
        QTest::newRow("ANAN-10") << int(HPSDRModel::ANAN10) << false << false << false << false;
        QTest::newRow("ANAN-10E") << int(HPSDRModel::ANAN10E) << false << false << false << false;
        QTest::newRow("ANAN-100") << int(HPSDRModel::ANAN100) << true << true << true << true;
        QTest::newRow("ANAN-100B") << int(HPSDRModel::ANAN100B) << true << true << true << true;
        QTest::newRow("ANAN-100D") << int(HPSDRModel::ANAN100D) << true << true << true << true;
        QTest::newRow("ANAN-200D") << int(HPSDRModel::ANAN200D) << true << true << true << true;
        QTest::newRow("Orion MKII") << int(HPSDRModel::ORIONMKII) << true << true << true << true;
        QTest::newRow("ANAN-7000D") << int(HPSDRModel::ANAN7000D) << false << true << true << false;
        QTest::newRow("ANAN-8000D") << int(HPSDRModel::ANAN8000D) << false << false << false << false;
        QTest::newRow("ANAN-G2") << int(HPSDRModel::ANAN_G2) << false << true << true << false;
        QTest::newRow("ANAN-G2-1K") << int(HPSDRModel::ANAN_G2_1K) << false << false << false << false;
        QTest::newRow("Anvelina Pro 3") << int(HPSDRModel::ANVELINAPRO3) << false << true << true << false;
        QTest::newRow("Hermes Lite") << int(HPSDRModel::HERMESLITE) << false << false << false << false;
        QTest::newRow("Red Pitaya") << int(HPSDRModel::REDPITAYA) << false << true << true << false;
        QTest::newRow("ANAN-G2E") << int(HPSDRModel::ANAN_G2E) << false << true << true << false;
    }

    void describedHardwareAntennaScalarsMatchDesktop()
    {
        QFETCH(int, model);
        QFETCH(bool, rxOut);
        QFETCH(bool, ext1);
        QFETCH(bool, ext2);
        QFETCH(bool, overrideRelay);
        const auto sku = static_cast<HPSDRModel>(model);
        RadioModel radio;
        radio.setHpsdrModelForTest(sku);
        AntennaAlexAntennaControlTab page(&radio);
        SetupDescriptionService service;
        service.setRadioContext(radio.boardCapabilities(), sku);
        const QJsonObject hardware = QJsonDocument::fromJson(
            SetupDescriptionService::fitCategoryForVersion(service.hardware(), 1).toUtf8())
            .object();
        if (!radio.boardCapabilities().hasAlexFilters) {
            QVERIFY(hardware.isEmpty());
            return;
        }
        QVERIFY(!hardware.isEmpty());
        const QJsonArray described = controls(hardware);
        QCOMPARE(described.size(), 3 + int(rxOut) + int(ext1) + int(ext2) + int(overrideRelay));
        QStringList expected{QStringLiteral("hardware.antennaAlex.blockTxAnt2"),
                             QStringLiteral("hardware.antennaAlex.blockTxAnt3")};
        if (rxOut) { expected << QStringLiteral("hardware.antennaAlex.rxOutOnTx"); }
        if (ext1) { expected << QStringLiteral("hardware.antennaAlex.ext1OutOnTx"); }
        if (ext2) { expected << QStringLiteral("hardware.antennaAlex.ext2OutOnTx"); }
        if (overrideRelay) { expected << QStringLiteral("hardware.antennaAlex.rxOutOverride"); }
        expected << QStringLiteral("hardware.antennaAlex.useTxAntennaForRx");
        QStringList actual;
        for (const QJsonValue& raw : described) {
            actual << raw.toObject().value(QStringLiteral("id")).toString();
            compareControl(page, raw.toObject());
            QVERIFY(SetupDescriptionService::validateHardwarePropertyBinding(raw.toObject(), sku));
        }
        QCOMPARE(actual, expected);
        for (const auto& [name, shown] : {
                 std::pair{"rxOutOnTx", rxOut}, std::pair{"ext1OutOnTx", ext1},
                 std::pair{"ext2OutOnTx", ext2}, std::pair{"rxOutOverride", overrideRelay}}) {
            const auto* widget = qobject_cast<QWidget*>(bySetupId(
                page, QStringLiteral("hardware.antennaAlex.") + QString::fromLatin1(name)));
            QVERIFY(widget != nullptr);
            QCOMPARE(!widget->isHidden(), shown);
        }
        if (sku == HPSDRModel::ANAN_G2E) {
            const QJsonObject ext2Control = described.at(3).toObject();
            QCOMPARE(ext2Control.value("label"), QJsonValue("Rx BYPASS on Tx"));
            QCOMPARE(ext2Control.value("tooltip"),
                     QJsonValue("Enable RX 1 IN on Alex or Ext 2 on ANAN during transmit."));
        }
    }

    void describedAntennaRowsMatchNativeGrid_data()
    {
        QTest::addColumn<int>("model");
        QTest::newRow("Hermes") << int(HPSDRModel::HERMES);
        QTest::newRow("ANAN-100") << int(HPSDRModel::ANAN100);
        QTest::newRow("ANAN-G2") << int(HPSDRModel::ANAN_G2);
        QTest::newRow("HL2-no-Alex") << int(HPSDRModel::HERMESLITE);
    }

    void describedAntennaRowsMatchNativeGrid()
    {
        QFETCH(int, model);
        const auto sku = static_cast<HPSDRModel>(model);
        RadioModel radio;
        radio.setHpsdrModelForTest(sku);
        AntennaAlexAntennaControlTab page(&radio);
        SetupDescriptionService service;
        service.setRadioContext(radio.boardCapabilities(), sku);
        // Version 12's projection: version 13 added pages around Antenna / ALEX.
        const QJsonObject hardware = projectedCategory(service.hardware(), 12);
        if (!radio.boardCapabilities().hasAlexFilters) {
            QVERIFY(hardware.isEmpty());
            return;
        }
        QCOMPARE(hardware.value("version"), QJsonValue(6));
        const QJsonArray described = controls(hardware);
        const QJsonObject tx = described.at(described.size() - 2).toObject();
        const QJsonObject rx = described.last().toObject();
        QCOMPARE(tx.value("id"), QJsonValue("hardware.antenna.txRows"));
        QCOMPARE(rx.value("id"), QJsonValue("hardware.antenna.rxRows"));
        const auto verify = [&page, sku](const QJsonObject& table) {
            QVERIFY(SetupDescriptionService::validateAntennaRowsTable(table, sku));
            auto* group = qobject_cast<QGroupBox*>(bySetupId(page,
                table.value("id").toString()));
            QVERIFY(group != nullptr);
            QCOMPARE(group->title(), table.value("label").toString());
            QCOMPARE(group->toolTip(), table.value("tooltip").toString());
            const QJsonArray columns = table.value("columns").toArray();
            const QJsonArray rows = table.value("rows").toArray();
            QCOMPARE(rows.size(), kPerBandStateCount);
            QStringList describedColumns;
            for (const QJsonValue& rawColumn : columns) {
                describedColumns << rawColumn.toObject().value("id").toString();
            }
            QStringList nativeColumns;
            for (QLabel* label : group->findChildren<QLabel*>()) {
                const QString id = label->property("nereusAntennaColumn").toString();
                if (!id.isEmpty()) { nativeColumns << id; }
            }
            QCOMPARE(nativeColumns, describedColumns);
            for (const QJsonValue& rawColumn : columns) {
                const QJsonObject column = rawColumn.toObject();
                QList<QLabel*> headers;
                for (QLabel* label : group->findChildren<QLabel*>()) {
                    if (label->property("nereusAntennaColumn")
                        == column.value("id").toVariant()) { headers.append(label); }
                }
                QCOMPARE(headers.size(), 1);
                QCOMPARE(headers.first()->text(), column.value("label").toString());
            }
            const QJsonArray columnGroups = table.value("columnGroups").toArray();
            QStringList nativeGroups;
            for (QLabel* label : group->findChildren<QLabel*>()) {
                const QString title = label->property("nereusAntennaColumnGroup").toString();
                if (!title.isEmpty()) { nativeGroups << title; }
            }
            QStringList describedGroups;
            for (const QJsonValue& rawGroup : columnGroups) {
                describedGroups << rawGroup.toObject().value("label").toString();
            }
            QCOMPARE(nativeGroups, describedGroups);
            for (const QJsonValue& rawGroup : columnGroups) {
                const QJsonObject header = rawGroup.toObject();
                QList<QLabel*> matches;
                for (QLabel* label : group->findChildren<QLabel*>()) {
                    if (label->property("nereusAntennaColumnGroup")
                        == header.value("label").toVariant()) { matches.append(label); }
                }
                QCOMPARE(matches.size(), 1);
                QCOMPARE(matches.first()->text(), header.value("label").toString());
            }
            // One row per antenna-list entry, in list order: 160m .. XVTR,
            // then 2 m (band 27, R-IOS-26).
            QCOMPARE(rows.size(), kPerBandStateCount);
            for (int slot = 0; slot < rows.size(); ++slot) {
                const QJsonObject row = rows.at(slot).toObject();
                const int band = static_cast<int>(bandFromPerBandStateSlot(slot));
                QCOMPARE(row.value("band"), QJsonValue(band));
                QList<QLabel*> labels;
                for (QLabel* label : group->findChildren<QLabel*>()) {
                    if (label->property("nereusAntennaRowLabel").toBool()
                        && label->property("nereusAntennaBand").toInt() == band) {
                        labels.append(label);
                    }
                }
                QCOMPARE(labels.size(), 1);
                QCOMPARE(labels.first()->text(), row.value("label").toString());
                const QJsonArray cells = row.value("cells").toArray();
                QCOMPARE(cells.size(), columns.size());
                QStringList nativeCellOrder;
                for (QRadioButton* button : group->findChildren<QRadioButton*>()) {
                    if (button->property("nereusAntennaBand").toInt() == band) {
                        nativeCellOrder << button->property("nereusAntennaColumn").toString();
                    }
                }
                QCOMPARE(nativeCellOrder, describedColumns);
                for (int i = 0; i < cells.size(); ++i) {
                    const QJsonObject cell = cells.at(i).toObject();
                    QCOMPARE(cell.value("column"), columns.at(i).toObject().value("id"));
                    QList<QRadioButton*> buttons;
                    for (QRadioButton* button : group->findChildren<QRadioButton*>()) {
                        if (button->property("nereusAntennaBand").toInt() == band
                            && button->property("nereusAntennaColumn")
                                == cell.value("column").toVariant()) {
                            buttons.append(button);
                        }
                    }
                    QCOMPARE(buttons.size(), 1);
                    QCOMPARE(buttons.first()->toolTip(), cell.value("tooltip").toString());
                }
            }
        };
        verify(tx);
        verify(rx);
        auto* rxGroup = qobject_cast<QGroupBox*>(bySetupId(page,
            QStringLiteral("hardware.antenna.rxRows")));
        QVERIFY(rxGroup != nullptr);
        for (const Band b : kPerBandStateBands) {
            const int band = static_cast<int>(b);
            int selected = 0;
            for (QRadioButton* button : rxGroup->findChildren<QRadioButton*>()) {
                if (button->property("nereusAntennaBand").toInt() == band
                    && button->property("nereusAntennaColumn").toString().startsWith(
                        QStringLiteral("rxOnly")) && button->isChecked()) {
                    ++selected;
                }
            }
            QCOMPARE(selected, radio.alexController().rxOnlyAnt(static_cast<Band>(band)) == 0
                                    ? 0 : 1);
        }
        auto* txGroup = qobject_cast<QGroupBox*>(bySetupId(page,
            QStringLiteral("hardware.antenna.txRows")));
        QVERIFY(txGroup != nullptr);
        radio.alexControllerMutable().setBlockTxAnt2(true);
        radio.alexControllerMutable().setBlockTxAnt3(true);
        for (QRadioButton* button : txGroup->findChildren<QRadioButton*>()) {
            const QString column = button->property("nereusAntennaColumn").toString();
            QCOMPARE(button->isEnabled(), column == QLatin1String("tx1"));
        }
    }

    void describedAntennaRowsRetargetOnSameBoardSkuChange()
    {
        RadioModel radio;
        radio.setHpsdrModelForTest(HPSDRModel::HERMES);
        AntennaAlexAntennaControlTab page(&radio);
        SetupDescriptionService service;
        service.setRadioContext(radio.boardCapabilities(), HPSDRModel::HERMES);
        const quint32 before = service.revision();
        const QString oldDescription = service.hardware();
        radio.setHpsdrModelForTest(HPSDRModel::ANAN100);
        radio.currentRadioChanged(radio.currentRadioInfo());
        service.setRadioContext(radio.boardCapabilities(), HPSDRModel::ANAN100);
        QVERIFY(service.revision() > before);
        QVERIFY(service.hardware() != oldDescription);
        const QJsonArray described = controls(projectedCategory(service.hardware(), 12));
        const QJsonObject rx = described.last().toObject();
        QVERIFY(SetupDescriptionService::validateAntennaRowsTable(rx, HPSDRModel::ANAN100));
        auto* group = qobject_cast<QGroupBox*>(bySetupId(page,
            QStringLiteral("hardware.antenna.rxRows")));
        QVERIFY(group != nullptr);
        const QJsonArray columns = rx.value("columns").toArray();
        for (int i = 3; i < 6; ++i) {
            const QString id = columns.at(i).toObject().value("id").toString();
            QLabel* header = nullptr;
            for (QLabel* label : group->findChildren<QLabel*>()) {
                if (label->property("nereusAntennaColumn").toString() == id) {
                    header = label;
                    break;
                }
            }
            QVERIFY(header != nullptr);
            QCOMPARE(header->text(), columns.at(i).toObject().value("label").toString());
            for (int band = 0; band < 14; ++band) {
                QRadioButton* button = nullptr;
                for (QRadioButton* candidate : group->findChildren<QRadioButton*>()) {
                    if (candidate->property("nereusAntennaBand").toInt() == band
                        && candidate->property("nereusAntennaColumn").toString() == id) {
                        button = candidate;
                        break;
                    }
                }
                QVERIFY(button != nullptr);
                const QJsonObject row = rx.value("rows").toArray().at(band).toObject();
                QCOMPARE(button->toolTip(), row.value("cells").toArray().at(i).toObject()
                             .value("tooltip").toString());
            }
        }
    }

    void describedTransmitDexpControlsMatchDesktop_data()
    {
        QTest::addColumn<int>("board");
        QTest::newRow("ANAN-G2") << int(HPSDRHW::Saturn);
        QTest::newRow("HL2") << int(HPSDRHW::HermesLite);
    }

    void describedAudioTxProfileControlsMatchDesktop_data()
    {
        QTest::addColumn<int>("board");
        QTest::addColumn<int>("radioMicRows");
        // Radio codec lane: the G2's Mic Tip-Ring (version 24), and the
        // HL2's Hermes rows through its audio add-on board.
        QTest::newRow("ANAN-G2") << int(HPSDRHW::Saturn) << 5;
        QTest::newRow("HL2") << int(HPSDRHW::HermesLite) << 3;
        QTest::newRow("Hermes") << int(HPSDRHW::Hermes) << 3;
        QTest::newRow("Orion-MkII") << int(HPSDRHW::OrionMKII) << 4;
    }

    void describedAudioTxProfileControlsMatchDesktop()
    {
        QFETCH(int, board);
        QFETCH(int, radioMicRows);
        RadioModel model;
        model.setBoardForTest(static_cast<HPSDRHW>(board));
        TxProfileSetupPage page(&model, nullptr, &model.transmitModel());
        AudioTxInputPage input(&model);
        SetupDescriptionService service;
        service.setBoardCapabilities(model.boardCapabilities());
        const QJsonObject audio = service.category(QStringLiteral("audio"));
        QVERIFY(!audio.isEmpty());
        const QJsonArray pages = audio.value(QStringLiteral("pages")).toArray();
        QCOMPARE(pages.size(), 2);
        QCOMPARE(pages.first().toObject().value(QStringLiteral("id")),
                 QJsonValue(QStringLiteral("audio.txInput")));
        QCOMPARE(pages.last().toObject().value(QStringLiteral("id")),
                 QJsonValue(QStringLiteral("audio.txProfile")));
        const QJsonArray described = controls(audio);
        QCOMPARE(described.size(), 7 + radioMicRows);
        for (const QJsonValue& raw : described) {
            const QJsonObject control = raw.toObject();
            compareControl(control.value("id").toString().startsWith("audio.txInput.")
                               ? static_cast<QWidget&>(input) : static_cast<QWidget&>(page),
                           control);
        }
    }

    void describedTransmitDexpControlsMatchDesktop()
    {
        QFETCH(int, board);
        RadioModel model;
        model.setBoardForTest(static_cast<HPSDRHW>(board));
        DexpVoxPage dexp(&model);
        PowerPage power(&model);
        SpeechProcessorPage speech(&model);
        SetupDescriptionService service;
        const QJsonObject transmit = service.category(QStringLiteral("transmit"));
        QVERIFY(!transmit.isEmpty());
        const QJsonArray pages = transmit.value(QStringLiteral("pages")).toArray();
        QCOMPARE(pages.size(), 3);
        QCOMPARE(pages.at(0).toObject().value(QStringLiteral("id")),
                 QJsonValue(QStringLiteral("transmit.power")));
        QCOMPARE(pages.at(1).toObject().value(QStringLiteral("id")),
                 QJsonValue(QStringLiteral("transmit.speechProcessor")));
        QCOMPARE(pages.at(2).toObject().value(QStringLiteral("id")),
                 QJsonValue(QStringLiteral("transmit.dexpVox")));
        const QJsonArray described = controls(transmit);
        QCOMPARE(described.size(), 46);
        for (const QJsonValue& raw : described) {
            const QJsonObject control = raw.toObject();
            const QString id = control.value(QStringLiteral("id")).toString();
            compareControl(id.startsWith(QStringLiteral("transmit.power.")) ? static_cast<QWidget&>(power)
                           : id.startsWith(QStringLiteral("transmit.speechProcessor."))
                             ? static_cast<QWidget&>(speech) : static_cast<QWidget&>(dexp), control);
        }
    }

    void describedGeneralAndTestControlsMatchDesktop_data()
    {
        QTest::addColumn<int>("board");
        QTest::newRow("ANAN-G2") << int(HPSDRHW::Saturn);
        QTest::newRow("HL2") << int(HPSDRHW::HermesLite);
    }

    void describedGeneralAndTestControlsMatchDesktop()
    {
        QFETCH(int, board);
        AppSettings::instance().clear();
        RadioModel model;
        model.setBoardForTest(static_cast<HPSDRHW>(board));
        model.addSlice();
        QVERIFY(model.notchModel() != nullptr);
        QVERIFY(model.notchModel()->addNotch(14074000.0) >= 0);
        // A real Setup page opens after connectToRadio has installed and
        // ranged the controller. Recreate that state without RF hardware.
        auto* attenuator = new StepAttenuatorController(&model);
        attenuator->setMinAttenuation(model.boardCapabilities().attenuator.minDb);
        attenuator->setMaxAttenuation(model.boardCapabilities().attenuator.maxDb);
        model.setStepAttController(attenuator);
        SetupDescriptionService service;
        service.setBoardCapabilities(model.boardCapabilities());
        StartupPrefsPage startup(&model);
        GeneralOptionsPage options(&model);
        TestTwoTonePage test(&model);
        CatTciServerPage cat;
        NrAnfSetupPage nr(&model);
        NbSnbSetupPage nb(&model);
        CwSetupPage cw(&model);
        AmSamSetupPage am(&model);
        FmSetupPage fm(&model);
        CfcSetupPage cfc(&model);
        AgcAlcSetupPage agc(&model);
        MnfSetupPage tnf(&model);
        DspOptionsPage dspOptions(&model);
        FilterPresetsSetupPage filterPresets(model.filterPresetStore(), &model);
        for (const QJsonValue& raw : controls(service.category(QStringLiteral("general")))) {
            const QJsonObject c = raw.toObject();
            QWidget& page = c.value("id").toString().startsWith("general.startup.")
                ? static_cast<QWidget&>(startup) : static_cast<QWidget&>(options);
            compareControl(page, c);
        }
        for (const QJsonValue& raw : controls(service.category(QStringLiteral("test")))) {
            compareControl(test, raw.toObject());
        }
        for (const QJsonValue& raw : controls(service.category(QStringLiteral("catNetwork")))) {
            // The version 15 rows: describedCatNetworkRowsMatchRemoteDesktop.
            if (raw.toObject().value("id").toString().startsWith("catNetwork.tciServer.")) {
                compareControl(cat, raw.toObject());
            }
        }
        const QJsonObject dsp = service.category(QStringLiteral("dsp"));
        QCOMPARE(dsp.value("pages").toArray().size(), 10);
        QStringList pageIds;
        for (const QJsonValue& page : dsp.value("pages").toArray()) {
            pageIds.append(page.toObject().value("id").toString());
        }
        QCOMPARE(pageIds, (QStringList{"dsp.agcAlc", "dsp.nrAnf", "dsp.nbSnb",
                                       "dsp.cw", "dsp.amSam", "dsp.fm", "dsp.cfc",
                                       "dsp.tnf", "dsp.filterPresets", "dsp.options"}));
        for (const QJsonValue& raw : controls(dsp)) {
            const QJsonObject c = raw.toObject();
            const QString id = c.value("id").toString();
            QWidget& page = id.startsWith("dsp.nrAnf.") ? static_cast<QWidget&>(nr)
                : id.startsWith("dsp.agcAlc.") ? static_cast<QWidget&>(agc)
                : id.startsWith("dsp.nbSnb.") ? static_cast<QWidget&>(nb)
                : id.startsWith("dsp.cw.") ? static_cast<QWidget&>(cw)
                : id.startsWith("dsp.amSam.") ? static_cast<QWidget&>(am)
                : id.startsWith("dsp.fm.") ? static_cast<QWidget&>(fm)
                : id.startsWith("dsp.options.") ? static_cast<QWidget&>(dspOptions)
                : id.startsWith("dsp.tnf.") ? static_cast<QWidget&>(tnf)
                : id.startsWith("dsp.filterPresets.") ? static_cast<QWidget&>(filterPresets)
                : static_cast<QWidget&>(cfc);
            compareControl(page, c);
        }
    }

    void nrAnfEditsFollowTheActiveSliceAndDisableWithoutOne()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Saturn);
        QCOMPARE(model.addSlice(), 0);
        QCOMPARE(model.addSlice(), 1);
        SliceModel* first = model.sliceById(0);
        SliceModel* second = model.sliceById(1);
        QVERIFY(first != nullptr);
        QVERIFY(second != nullptr);
        QVERIFY(model.setActiveSliceById(0));
        NrAnfSetupPage page(&model);
        auto taps = [&page]() {
            return qobject_cast<QSlider*>(bySetupId(page, QStringLiteral("dsp.nrAnf.nr1Taps")));
        };
        QVERIFY(taps() != nullptr);
        taps()->setValue(80);
        QCOMPARE(first->nr1Taps(), 80);
        const int secondOld = second->nr1Taps();
        QPointer<QSlider> oldGestureControl(taps());
        QVERIFY(model.setActiveSliceById(1));
        QVERIFY(oldGestureControl.isNull()); // a pending drag cannot retarget B
        QVERIFY(taps() != nullptr);
        auto* position = qobject_cast<QButtonGroup*>(bySetupId(
            page, QStringLiteral("dsp.nrAnf.nr1Position")));
        QVERIFY(position != nullptr);
        QCOMPARE(position->buttons().size(), 2);
        QCOMPARE(taps()->value(), secondOld);
        taps()->setValue(96);
        QCOMPARE(second->nr1Taps(), 96);
        QCOMPARE(first->nr1Taps(), 80);
        model.removeSlice(1);
        QVERIFY(taps() != nullptr);
        taps()->setValue(104);
        QCOMPARE(first->nr1Taps(), 104);
        // A local Core deliberately retains its last slice. A fresh model
        // supplies the no-selection state without changing that invariant.
        RadioModel empty;
        NrAnfSetupPage withoutSlice(&empty);
        auto* unavailable = qobject_cast<QSlider*>(bySetupId(
            withoutSlice, QStringLiteral("dsp.nrAnf.nr1Taps")));
        QVERIFY(unavailable != nullptr);
        QVERIFY(!unavailable->isEnabledTo(&withoutSlice));
    }

    void agcEditsFollowTheSelectedSliceAndRetireOldWidgets()
    {
        RadioModel model;
        QCOMPARE(model.addSlice(), 0);
        QCOMPARE(model.addSlice(), 1);
        SliceModel* first = model.sliceById(0);
        SliceModel* second = model.sliceById(1);
        QVERIFY(first != nullptr);
        QVERIFY(second != nullptr);
        QVERIFY(model.setActiveSliceById(0));
        AgcAlcSetupPage page(&model);
        auto attack = [&page]() {
            return qobject_cast<QSpinBox*>(bySetupId(
                page, QStringLiteral("dsp.agcAlc.agcAttack")));
        };
        QVERIFY(attack() != nullptr);
        attack()->setValue(37);
        QCOMPARE(first->agcAttack(), 37);
        QPointer<QSpinBox> old(attack());
        QVERIFY(model.setActiveSliceById(1));
        QVERIFY(old.isNull());
        QVERIFY(attack() != nullptr);
        attack()->setValue(53);
        QCOMPARE(second->agcAttack(), 53);
        QCOMPARE(first->agcAttack(), 37);
    }
};

QTEST_MAIN(SetupDescriptionParityTest)
#include "tst_setup_description_parity.moc"
