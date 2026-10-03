// no-port-check: NereusSDR-original test.
// =================================================================
// tests/tst_catalogue_ranges.cpp  (NereusSDR)
// =================================================================
//
// The Core's catalogue carries what the desktop's controls offer (iPhone
// app plan, R-IOS-06, R-IOS-27), for the controls the phone draws from
// it:
//
//   - board.transmit: the TX applet's RF Power and Tune sliders, Setup >
//     Transmit > Power's fixed tune spinbox and the Phone/CW applet's mic
//     level, on the G2 and the HL2, against the widgets themselves (the
//     HL2's shown dB by mi0bot console.cs:29245 and setup.cs:5307).
//   - board.rx1Preamp and board.relays: which of the RX1 preamp, RX out on
//     TX and the Ext-on-TX switches the radio has (the desktop hides the
//     rest), from the gates the desktop uses.
//   - noiseReduction: every NR quick control, against the VFO flag's
//     popups, NnrControls, a new slice's values and the mirror policy.
//
// No radio is connected, nothing keys and no audio device opens.
//
//   cmake --build build --target tst_catalogue_ranges
//   QT_QPA_PLATFORM=offscreen ctest --test-dir build -R '^tst_catalogue_ranges$' \
//       --output-on-failure
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: original test for NereusSDR by J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code
//               (R-IOS-06, R-IOS-27).
//   2026-09-27: shown.rounding, and the TX applet's labels at every value
//               between the steps (mi0bot's HL2 drive snap). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-27: shown.endSnap and `down`; the tune label and the Power
//               page's spinbox at every value (mi0bot's HL2 tune readouts).
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-30: board.rx2Attenuator, rx2PreampItems and rx2AttenuatorReason
//               on every radio (Level Cal 2). J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QRadioButton>
#include <QSet>
#include <QSlider>

#include <cmath>

#include "core/BoardCapabilities.h"
#include "core/ControlRanges.h"
#include "core/HpsdrModel.h"
#include "core/SkuUiProfile.h"
#include "core/StepAttenuatorController.h"
#include "core/session/MirrorPolicy.h"
#include "core/session/StationCatalog.h"
#include "gui/AntennaPopupBuilder.h"
#include "gui/applets/TxApplet.h"
#include "gui/setup/TransmitSetupPages.h"
#include "gui/widgets/DspParamPopup.h"
#include "gui/widgets/NnrControls.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include "OperatorWording.h"

using namespace NereusSDR;

namespace {

QJsonObject catalogueFor(HPSDRModel model)
{
    StationCatalog::Inputs inputs;
    inputs.model = model;
    inputs.board = BoardCapsTable::forModel(model);
    inputs.protocol = inputs.board.protocol;
    return StationCatalog::build(inputs);
}

QJsonObject boardOf(HPSDRModel model)
{
    return catalogueFor(model).value(QStringLiteral("board")).toObject();
}

QJsonObject range(double min, double max, double step)
{
    return QJsonObject{{QStringLiteral("min"), min},
                       {QStringLiteral("max"), max},
                       {QStringLiteral("step"), step}};
}

QJsonObject rangeShown(double min, double max, double step, double shownMin, double shownMax,
                       int decimals, const QString& unit,
                       const QString& rounding = QStringLiteral("none"),
                       const QJsonValue& endSnap = QJsonValue(QJsonValue::Null))
{
    QJsonObject out = range(min, max, step);
    out.insert(QStringLiteral("shown"),
               QJsonObject{{QStringLiteral("min"), shownMin},
                           {QStringLiteral("max"), shownMax},
                           {QStringLiteral("decimals"), decimals},
                           {QStringLiteral("unit"), unit},
                           {QStringLiteral("rounding"), rounding},
                           {QStringLiteral("endSnap"), endSnap}});
    return out;
}

QJsonObject ends(int below, int above)
{
    return QJsonObject{{QStringLiteral("below"), below}, {QStringLiteral("above"), above}};
}

// A half to the even whole number, as the catalogue's `halfEven` says.
double roundHalfEven(double x)
{
    const double down = std::floor(x);
    const double diff = x - down;
    if (diff != 0.5) {
        return std::round(x);
    }
    return std::fmod(down, 2.0) == 0.0 ? down : down + 1.0;
}

// What the control shows at `value`, by the catalogue's rule: a value
// below shown.endSnap.below shows as `min` and one above shown.endSnap.above
// as `max`; then shown.rounding takes a value between steps to a step
// (`halfEven`: the nearest, a half to the even step; `down`: the step at or
// below it; `none`: no step); then linear from shown.min at `min` to
// shown.max at `max`, to `decimals` places.
double shownValueByTheCatalogue(const QJsonObject& control, int value)
{
    const double min = control.value(QStringLiteral("min")).toDouble();
    const double max = control.value(QStringLiteral("max")).toDouble();
    const double step = control.value(QStringLiteral("step")).toDouble();
    const QJsonObject shown = control.value(QStringLiteral("shown")).toObject();
    double v = value;
    const QJsonValue ends = shown.value(QStringLiteral("endSnap"));
    if (ends.isObject()) {
        if (v < ends.toObject().value(QStringLiteral("below")).toDouble()) {
            v = min;
        } else if (v > ends.toObject().value(QStringLiteral("above")).toDouble()) {
            v = max;
        }
    }
    const QString rounding = shown.value(QStringLiteral("rounding")).toString();
    if (rounding == QStringLiteral("halfEven")) {
        v = min + step * roundHalfEven((v - min) / step);
    } else if (rounding == QStringLiteral("down")) {
        v = min + step * std::floor((v - min) / step);
    }
    const double lo = shown.value(QStringLiteral("min")).toDouble();
    const double hi = shown.value(QStringLiteral("max")).toDouble();
    return lo + (v - min) * (hi - lo) / (max - min);
}

QString shownByTheCatalogue(const QJsonObject& control, int value)
{
    return QString::number(shownValueByTheCatalogue(control, value), 'f',
                           control.value(QStringLiteral("shown")).toObject()
                               .value(QStringLiteral("decimals")).toInt());
}

// A popup's rows: sliders by label (label, slider, readout), and the texts
// of its radio buttons and checkboxes.
struct PopupRows {
    QMap<QString, QSlider*> sliders;
    QMap<QString, QLabel*> readouts;
    QStringList choices;
    QStringList switches;
};

PopupRows rowsOf(const DspParamPopup& popup)
{
    PopupRows rows;
    QLayout* layout = popup.layout();
    for (int i = 0; layout && i < layout->count(); ++i) {
        QLayout* row = layout->itemAt(i)->layout();
        if (!row || row->count() != 3) {
            continue;
        }
        auto* label = qobject_cast<QLabel*>(row->itemAt(0)->widget());
        auto* slider = qobject_cast<QSlider*>(row->itemAt(1)->widget());
        auto* readout = qobject_cast<QLabel*>(row->itemAt(2)->widget());
        if (label && slider && readout) {
            rows.sliders.insert(label->text(), slider);
            rows.readouts.insert(label->text(), readout);
        }
    }
    for (const QRadioButton* button : popup.findChildren<QRadioButton*>()) {
        rows.choices.append(button->text());
    }
    for (const QCheckBox* box : popup.findChildren<QCheckBox*>()) {
        rows.switches.append(box->text());
    }
    return rows;
}

DspParamPopup* openPopup(VfoWidget& vfo, const QString& buttonText)
{
    for (QPushButton* button : vfo.findChildren<QPushButton*>()) {
        if (button->text() == buttonText) {
            emit button->customContextMenuRequested(QPoint(1, 1));
            const QList<DspParamPopup*> popups = vfo.findChildren<DspParamPopup*>();
            return popups.isEmpty() ? nullptr : popups.last();
        }
    }
    return nullptr;
}

QJsonArray controlsOf(const QString& slot)
{
    return catalogueFor(HPSDRModel::ANAN_G2)
        .value(QStringLiteral("noiseReduction"))
        .toObject()
        .value(slot)
        .toArray();
}

// The catalogue's controls for `slot` against the popup the VFO flag opens
// for it on a new slice.
void checkPopupAgainstCatalogue(const QString& slot, const QString& button)
{
    RadioModel model;
    model.addSlice();
    SliceModel* slice = model.activeSlice();
    QVERIFY(slice);
    VfoWidget vfo;
    vfo.setRadioModel(&model);
    vfo.setSlice(slice);
    DspParamPopup* popup = openPopup(vfo, button);
    QVERIFY2(popup, qPrintable(QStringLiteral("no popup for %1").arg(button)));
    const PopupRows rows = rowsOf(*popup);

    QStringList catalogueSliders;
    QStringList catalogueChoices;
    QStringList catalogueSwitches;
    for (const QJsonValue& value : controlsOf(slot)) {
        const QJsonObject control = value.toObject();
        const QString label = control.value(QStringLiteral("label")).toString();
        const QString kind = control.value(QStringLiteral("kind")).toString();
        if (kind == QStringLiteral("slider")) {
            catalogueSliders.append(label);
            QSlider* slider = rows.sliders.value(label);
            QVERIFY2(slider, qPrintable(QStringLiteral("%1 has no %2 slider").arg(slot, label)));
            QCOMPARE(double(slider->minimum()), control.value(QStringLiteral("min")).toDouble());
            QCOMPARE(double(slider->maximum()), control.value(QStringLiteral("max")).toDouble());
            QCOMPARE(double(slider->singleStep()),
                     control.value(QStringLiteral("step")).toDouble());
            // On a new slice the slider sits at the default and reads it as
            // the catalogue says: slider / divide, places, suffix.
            const double scale = control.value(QStringLiteral("scale")).toDouble();
            const int position = static_cast<int>(
                std::lround(control.value(QStringLiteral("default")).toDouble() / scale));
            QCOMPARE(slider->value(), position);
            const QString readout =
                QString::number(double(position) / control.value(QStringLiteral("divide")).toInt(),
                                'f', control.value(QStringLiteral("decimals")).toInt())
                + control.value(QStringLiteral("suffix")).toString();
            QCOMPARE(rows.readouts.value(label)->text(), readout);
        } else if (kind == QStringLiteral("choice")) {
            for (const QJsonValue& option : control.value(QStringLiteral("options")).toArray()) {
                catalogueChoices.append(option.toObject().value(QStringLiteral("label")).toString());
            }
        } else {
            catalogueSwitches.append(label);
        }
    }
    QCOMPARE(QSet<QString>(catalogueSliders.cbegin(), catalogueSliders.cend()),
             QSet<QString>(rows.sliders.keyBegin(), rows.sliders.keyEnd()));
    QCOMPARE(catalogueChoices, rows.choices);
    QCOMPARE(catalogueSwitches, rows.switches);

    // Reset (DFNR and MNR alone): each slider goes to the catalogue's
    // `reset`, and the slice holds slider x scale.
    bool anyReset = false;
    for (const QJsonValue& value : controlsOf(slot)) {
        const QJsonObject control = value.toObject();
        if (control.value(QStringLiteral("kind")).toString() == QStringLiteral("slider")
            && !control.value(QStringLiteral("reset")).isNull()) {
            anyReset = true;
            QSlider* slider = rows.sliders.value(control.value(QStringLiteral("label")).toString());
            slider->setValue(slider->maximum());
        }
    }
    if (anyReset) {
        QPushButton* reset = nullptr;
        for (QPushButton* button : popup->findChildren<QPushButton*>()) {
            if (button->text() == QStringLiteral("Reset")) {
                reset = button;
            }
        }
        QVERIFY(reset);
        reset->click();
        for (const QJsonValue& value : controlsOf(slot)) {
            const QJsonObject control = value.toObject();
            if (control.value(QStringLiteral("reset")).isNull()
                || control.value(QStringLiteral("kind")).toString() != QStringLiteral("slider")) {
                continue;
            }
            const QString label = control.value(QStringLiteral("label")).toString();
            const int reset = control.value(QStringLiteral("reset")).toInt();
            QCOMPARE(rows.sliders.value(label)->value(), reset);
            const double written =
                slice->property(control.value(QStringLiteral("property")).toString().toLatin1())
                    .toDouble();
            QVERIFY2(std::abs(written - reset * control.value(QStringLiteral("scale")).toDouble())
                         < 1e-9,
                     qPrintable(QStringLiteral("%1 %2 wrote %3").arg(slot, label).arg(written)));
        }
    }
}

} // namespace

class TstCatalogueRanges : public QObject {
    Q_OBJECT

private slots:
    // ── board.transmit ──────────────────────────────────────────────────

    // The values, as the widgets offer them on each radio.
    void transmitValues()
    {
        const QJsonObject g2 = boardOf(HPSDRModel::ANAN_G2).value(QStringLiteral("transmit")).toObject();
        const QJsonObject hl2 =
            boardOf(HPSDRModel::HERMESLITE).value(QStringLiteral("transmit")).toObject();
        const QStringList keys = g2.keys();
        QCOMPARE(QSet<QString>(keys.cbegin(), keys.cend()),
                 (QSet<QString>{QStringLiteral("power"), QStringLiteral("tunePowerForTxBand"),
                                QStringLiteral("tunePower"), QStringLiteral("micGainDb")}));

        QCOMPARE(g2.value(QStringLiteral("power")).toObject(),
                 rangeShown(0, 100, 1, 0, 100, 0, QString(), QStringLiteral("halfEven")));
        QCOMPARE(g2.value(QStringLiteral("tunePowerForTxBand")).toObject(),
                 rangeShown(0, 100, 1, 0, 100, 0, QString()));
        QCOMPARE(g2.value(QStringLiteral("tunePower")).toObject(),
                 rangeShown(0, 100, 1, 0, 100, 0, QStringLiteral("W")));
        QCOMPARE(g2.value(QStringLiteral("micGainDb")).toObject(), range(-40, 10, 1));

        // The HL2's attenuator in dB (mi0bot console.cs:29245, setup.cs:5307).
        // A drive between steps shows as mi0bot's label snaps it (the nearest
        // step, a half to the even one; console.cs:29245-29264).
        QCOMPARE(hl2.value(QStringLiteral("power")).toObject(),
                 rangeShown(0, 90, 6, -7.5, 0, 1, QStringLiteral("dB"),
                            QStringLiteral("halfEven"), ends(4, 87)));
        // The tune label snaps and rounds as mi0bot's UpdateTuneLabel
        // (console.cs:47470-47481); the fixed tune spinbox takes C#'s
        // integer division (setup.cs:5307), the step at or below.
        QCOMPARE(hl2.value(QStringLiteral("tunePowerForTxBand")).toObject(),
                 rangeShown(0, 99, 3, -16.5, 0, 1, QStringLiteral("dB"),
                            QStringLiteral("halfEven"), ends(3, 96)));
        QCOMPARE(hl2.value(QStringLiteral("tunePower")).toObject(),
                 rangeShown(0, 99, 3, -16.5, 0, 1, QStringLiteral("dB"),
                            QStringLiteral("down")));
        QCOMPARE(hl2.value(QStringLiteral("micGainDb")).toObject(), range(-40, 10, 1));

        // The mic range is the board's; only the Unknown board is wider.
        for (int m = static_cast<int>(HPSDRModel::FIRST) + 1;
             m < static_cast<int>(HPSDRModel::LAST); ++m) {
            const auto model = static_cast<HPSDRModel>(m);
            const BoardCapabilities& caps = BoardCapsTable::forModel(model);
            QCOMPARE(boardOf(model).value(QStringLiteral("transmit")).toObject()
                         .value(QStringLiteral("micGainDb")).toObject(),
                     range(caps.micGainMinDb, caps.micGainMaxDb, 1));
        }
        const BoardCapabilities& unknown = BoardCapsTable::forBoard(HPSDRHW::Unknown);
        QCOMPARE(unknown.micGainMinDb, -50);
        QCOMPARE(unknown.micGainMaxDb, 70);
    }

    void transmitMatchesTheTxApplet_data()
    {
        QTest::addColumn<int>("model");
        QTest::newRow("ANAN-G2") << static_cast<int>(HPSDRModel::ANAN_G2);
        QTest::newRow("HL2") << static_cast<int>(HPSDRModel::HERMESLITE);
    }

    // The TX applet's sliders span the catalogue's ranges, and their labels
    // read what the catalogue's `shown` gives at every value.
    void transmitMatchesTheTxApplet()
    {
        QFETCH(int, model);
        const auto m = static_cast<HPSDRModel>(model);
        const QJsonObject transmit = boardOf(m).value(QStringLiteral("transmit")).toObject();
        RadioModel radio;
        TxApplet applet(&radio);
        applet.rescalePowerSlidersForModel(m);

        const struct {
            const char* key;
            QSlider* slider;
            QLabel* label;
        } rows[] = {
            {"power", applet.rfPowerSlider(), applet.rfPowerLabel()},
            {"tunePowerForTxBand", applet.tunePowerSlider(), applet.tunePowerLabel()},
        };
        for (const auto& row : rows) {
            const QJsonObject control = transmit.value(QString::fromLatin1(row.key)).toObject();
            QCOMPARE(double(row.slider->minimum()), control.value(QStringLiteral("min")).toDouble());
            QCOMPARE(double(row.slider->maximum()), control.value(QStringLiteral("max")).toDouble());
            const int step = control.value(QStringLiteral("step")).toInt();
            QCOMPARE(row.slider->singleStep(), step);
            // Every value, between the steps too (a drag, or another
            // window's write).
            for (int v = row.slider->minimum(); v <= row.slider->maximum(); ++v) {
                row.slider->setValue(v);
                applet.updatePowerSliderLabels();
                QCOMPARE(row.label->text(), shownByTheCatalogue(control, v));
            }
        }
    }

    void transmitMatchesThePowerPage_data()
    {
        transmitMatchesTheTxApplet_data();
    }

    // Setup > Transmit > Power's fixed tune spinbox shows the catalogue's
    // `shown` range, and its two ends write the stored min and max.
    void transmitMatchesThePowerPage()
    {
        QFETCH(int, model);
        const auto m = static_cast<HPSDRModel>(model);
        const QJsonObject tunePower = boardOf(m)
                                          .value(QStringLiteral("transmit"))
                                          .toObject()
                                          .value(QStringLiteral("tunePower"))
                                          .toObject();
        const QJsonObject shown = tunePower.value(QStringLiteral("shown")).toObject();
        RadioModel radio;
        radio.setHpsdrModelForTest(m);
        PowerPage page(&radio);
        auto* spin = page.findChild<QDoubleSpinBox*>(QStringLiteral("udTXTunePower"));
        QVERIFY(spin);
        QCOMPARE(spin->minimum(), shown.value(QStringLiteral("min")).toDouble());
        QCOMPARE(spin->maximum(), shown.value(QStringLiteral("max")).toDouble());
        QCOMPARE(spin->decimals(), shown.value(QStringLiteral("decimals")).toInt());
        QCOMPARE(spin->suffix().trimmed(), shown.value(QStringLiteral("unit")).toString());

        // Every stored value, between the steps too, shows as the catalogue
        // says.
        QStringList got;
        QStringList want;
        for (int v = tunePower.value(QStringLiteral("min")).toInt();
             v <= tunePower.value(QStringLiteral("max")).toInt(); ++v) {
            radio.transmitModel().setTunePower(v);
            got << QStringLiteral("%1:%2").arg(v).arg(spin->value(), 0, 'f', spin->decimals());
            want << QStringLiteral("%1:%2").arg(v).arg(shownByTheCatalogue(tunePower, v));
        }
        QCOMPARE(got.join(QLatin1Char(' ')), want.join(QLatin1Char(' ')));

        spin->setValue(spin->maximum());
        QCOMPARE(radio.transmitModel().tunePower(), tunePower.value(QStringLiteral("max")).toInt());
        spin->setValue(spin->minimum());
        QCOMPARE(radio.transmitModel().tunePower(), tunePower.value(QStringLiteral("min")).toInt());
        spin->setValue(spin->minimum() + spin->singleStep());
        QCOMPARE(radio.transmitModel().tunePower(),
                 tunePower.value(QStringLiteral("min")).toInt()
                     + tunePower.value(QStringLiteral("step")).toInt());
    }

    // ── board.rx1Preamp and board.relays ───────────────────────────────

    void boardPresenceFlags()
    {
        const QJsonObject g2 = boardOf(HPSDRModel::ANAN_G2);
        const QJsonObject hl2 = boardOf(HPSDRModel::HERMESLITE);
        QCOMPARE(g2.value(QStringLiteral("rx1Preamp")).toBool(), false);
        QCOMPARE(hl2.value(QStringLiteral("rx1Preamp")).toBool(), false);
        QCOMPARE(g2.value(QStringLiteral("relays")).toObject(),
                 (QJsonObject{{QStringLiteral("rxOutOnTx"), false},
                              {QStringLiteral("ext1OutOnTx"), QStringLiteral("Ext 1 on Tx")},
                              {QStringLiteral("ext2OutOnTx"), QStringLiteral("Ext 2 on Tx")},
                              {QStringLiteral("rxOutOverride"), false}}));
        QCOMPARE(hl2.value(QStringLiteral("relays")).toObject(),
                 (QJsonObject{{QStringLiteral("rxOutOnTx"), false},
                              {QStringLiteral("ext1OutOnTx"), QJsonValue(QJsonValue::Null)},
                              {QStringLiteral("ext2OutOnTx"), QJsonValue(QJsonValue::Null)},
                              {QStringLiteral("rxOutOverride"), false}}));
        // The G2E relabels Ext 2 on TX (setup.cs:19929 [v2.10.3.15]).
        QCOMPARE(boardOf(HPSDRModel::ANAN_G2E)
                     .value(QStringLiteral("relays")).toObject()
                     .value(QStringLiteral("ext2OutOnTx")).toString(),
                 skuUiProfileFor(HPSDRModel::ANAN_G2E).ext2OutOnTxLabel);
        // An ANAN-100D has RX out on TX and the RX out override.
        const QJsonObject anan100d =
            boardOf(HPSDRModel::ANAN100D).value(QStringLiteral("relays")).toObject();
        QCOMPARE(anan100d.value(QStringLiteral("rxOutOnTx")).toBool(), true);
        QCOMPARE(anan100d.value(QStringLiteral("rxOutOverride")).toBool(), true);

        // Every radio: the flags are the desktop's gates.
        for (int m = static_cast<int>(HPSDRModel::FIRST) + 1;
             m < static_cast<int>(HPSDRModel::LAST); ++m) {
            const auto model = static_cast<HPSDRModel>(m);
            const BoardCapabilities& caps = BoardCapsTable::forModel(model);
            const SkuUiProfile sku = skuUiProfileFor(model);
            const QJsonObject board = boardOf(model);
            // RxApplet shows the RX1 preamp toggle on p2PreampPerAdc.
            QCOMPARE(board.value(QStringLiteral("rx1Preamp")).toBool(), caps.p2PreampPerAdc);
            const QJsonObject relays = board.value(QStringLiteral("relays")).toObject();
            // RX out on TX: the VFO flag's BYPS gate, the antenna popup's.
            QCOMPARE(relays.value(QStringLiteral("rxOutOnTx")).toBool(),
                     caps.hasRxBypassRelay && sku.hasRxOutOnTx);
            QCOMPARE(relays.value(QStringLiteral("rxOutOnTx")).toBool(),
                     AntennaPopupBuilder::labels(caps, sku, AntennaPopupBuilder::Mode::RX)
                         .contains(QStringLiteral("RX out on TX")));
            // Setup > Antenna Control's Ext-on-TX boxes and RX out override.
            QCOMPARE(relays.value(QStringLiteral("ext1OutOnTx")).isNull(), !sku.hasExt1OutOnTx);
            QCOMPARE(relays.value(QStringLiteral("ext2OutOnTx")).isNull(), !sku.hasExt2OutOnTx);
            if (sku.hasExt1OutOnTx) {
                QCOMPARE(relays.value(QStringLiteral("ext1OutOnTx")).toString(),
                         sku.ext1OutOnTxLabel);
            }
            if (sku.hasExt2OutOnTx) {
                QCOMPARE(relays.value(QStringLiteral("ext2OutOnTx")).toString(),
                         sku.ext2OutOnTxLabel);
            }
            QCOMPARE(relays.value(QStringLiteral("rxOutOverride")).toBool(), sku.hasRxBypassUi);
        }
    }

    // ── board.rx2Attenuator (Level Cal 2) ──────────────────────────────

    // RX2's own input control, every step the hardware has (JJ's ruling of
    // 2026-09-30): the second ADC's 0-31 dB attenuator in 1 dB steps on the
    // two-ADC radios, the second Mercury's two states on the HPSDR, and on
    // every other radio neither, with the reason in operator words.
    void rx2InputControlIsTheHardwaresSteps()
    {
        const QString shares =
            QStringLiteral("RX2 uses RX1's input on this radio. Set it with RX1's attenuator.");
        const QString unknown = QStringLiteral("NereusSDR cannot set RX2's input on this radio.");
        QVERIFY(OperatorWording::isPlain(shares));
        QVERIFY(OperatorWording::isPlain(unknown));
        const QList<HPSDRModel> twoAdcs{
            HPSDRModel::ANAN100D, HPSDRModel::ANAN200D, HPSDRModel::ORIONMKII,
            HPSDRModel::ANAN7000D, HPSDRModel::ANAN8000D, HPSDRModel::ANVELINAPRO3,
            HPSDRModel::ANAN_G2, HPSDRModel::ANAN_G2_1K};
        for (int m = static_cast<int>(HPSDRModel::FIRST) + 1;
             m < static_cast<int>(HPSDRModel::LAST); ++m) {
            const auto model = static_cast<HPSDRModel>(m);
            const QJsonObject board = boardOf(model);
            const QJsonValue slider = board.value(QStringLiteral("rx2Attenuator"));
            const QJsonArray items = board.value(QStringLiteral("rx2PreampItems")).toArray();
            const QJsonValue reason = board.value(QStringLiteral("rx2AttenuatorReason"));
            QVERIFY2(board.contains(QStringLiteral("rx2Attenuator"))
                         && board.contains(QStringLiteral("rx2PreampItems"))
                         && board.contains(QStringLiteral("rx2AttenuatorReason")),
                     qPrintable(QString::number(m)));
            if (twoAdcs.contains(model)) {
                QCOMPARE(slider.toObject(), range(0, 31, 1));
                QVERIFY(items.isEmpty());
                QVERIFY(reason.isNull());
            } else if (model == HPSDRModel::HPSDR) {
                QVERIFY(slider.isNull());
                QCOMPARE(items, (QJsonArray{
                    QJsonObject{{QStringLiteral("id"), static_cast<int>(PreampMode::On)},
                                {QStringLiteral("label"), QStringLiteral("0dB")}},
                    QJsonObject{{QStringLiteral("id"), static_cast<int>(PreampMode::Off)},
                                {QStringLiteral("label"), QStringLiteral("-20dB")}}}));
                QVERIFY(reason.isNull());
            } else {
                QVERIFY(slider.isNull());
                QVERIFY(items.isEmpty());
                QCOMPARE(reason.toString(),
                         model == HPSDRModel::REDPITAYA ? unknown : shares);
            }
        }
    }

    // ── noiseReduction ─────────────────────────────────────────────────

    // The same on every radio; each control is a slice property a device
    // may write, and its default is a new slice's value.
    void noiseReductionIsTheSlicesControls()
    {
        const QJsonObject nr =
            catalogueFor(HPSDRModel::ANAN_G2).value(QStringLiteral("noiseReduction")).toObject();
        QCOMPARE(nr, catalogueFor(HPSDRModel::HERMESLITE)
                         .value(QStringLiteral("noiseReduction")).toObject());
        QCOMPARE(nr.keys(),
                 (QStringList{QStringLiteral("dfnr"), QStringLiteral("mnr"),
                              QStringLiteral("nnr"), QStringLiteral("nr1"), QStringLiteral("nr2"),
                              QStringLiteral("nr3"), QStringLiteral("nr4")}));
        const SliceModel fresh;
        int count = 0;
        for (const QString& slot : nr.keys()) {
            for (const QJsonValue& value : nr.value(slot).toArray()) {
                ++count;
                const QJsonObject control = value.toObject();
                const QByteArray property =
                    control.value(QStringLiteral("property")).toString().toLatin1();
                QVERIFY2(MirrorPolicy::inboundAllowed("SliceModel", property),
                         property.constData());
                const QVariant now = fresh.property(property.constData());
                QVERIFY2(now.isValid(), property.constData());
                const QString kind = control.value(QStringLiteral("kind")).toString();
                if (kind == QStringLiteral("switch")) {
                    QCOMPARE(control.value(QStringLiteral("default")).toBool(), now.toBool());
                    QCOMPARE(control.keys().size(), 4);
                } else if (kind == QStringLiteral("choice")) {
                    QCOMPARE(control.value(QStringLiteral("default")).toInt(), now.toInt());
                    QVERIFY(!control.value(QStringLiteral("options")).toArray().isEmpty());
                } else {
                    QCOMPARE(kind, QStringLiteral("slider"));
                    QVERIFY2(std::abs(control.value(QStringLiteral("default")).toDouble()
                                      - now.toDouble()) < 1e-12,
                             property.constData());
                    const QStringList keys = control.keys();
                    QCOMPARE(QSet<QString>(keys.cbegin(), keys.cend()),
                             (QSet<QString>{QStringLiteral("property"), QStringLiteral("label"),
                                            QStringLiteral("kind"), QStringLiteral("min"),
                                            QStringLiteral("max"), QStringLiteral("step"),
                                            QStringLiteral("scale"), QStringLiteral("divide"),
                                            QStringLiteral("decimals"), QStringLiteral("suffix"),
                                            QStringLiteral("default"), QStringLiteral("reset")}));
                }
            }
        }
        QCOMPARE(count, 5 + 6 + 2 + 6 + 2 + 6 + 9);

        // The corrected values: NR1 is Thetis's NR spinboxes, MNR's Reset
        // restores MacNRFilter's DEF_*.
        const QJsonArray nr1 = nr.value(QStringLiteral("nr1")).toArray();
        QCOMPARE(nr1.at(2).toObject().value(QStringLiteral("min")).toInt(), 1);
        QCOMPARE(nr1.at(2).toObject().value(QStringLiteral("max")).toInt(), 1000);
        QCOMPARE(nr1.at(2).toObject().value(QStringLiteral("scale")).toDouble(), 1e-6);
        QCOMPARE(nr1.at(3).toObject().value(QStringLiteral("scale")).toDouble(), 1e-3);
        const QJsonArray mnr = nr.value(QStringLiteral("mnr")).toArray();
        QCOMPARE(mnr.at(1).toObject().value(QStringLiteral("reset")).toInt(), 4);
        QCOMPARE(mnr.at(4).toObject().value(QStringLiteral("reset")).toInt(), 12);
    }

    // The VFO flag's popups offer exactly what the catalogue says.
    void noiseReductionMatchesThePopups()
    {
        checkPopupAgainstCatalogue(QStringLiteral("nr1"), QStringLiteral("NR1"));
        if (QTest::currentTestFailed()) { return; }
        checkPopupAgainstCatalogue(QStringLiteral("nr2"), QStringLiteral("NR2"));
        if (QTest::currentTestFailed()) { return; }
        checkPopupAgainstCatalogue(QStringLiteral("nr3"), QStringLiteral("NR3"));
        if (QTest::currentTestFailed()) { return; }
        checkPopupAgainstCatalogue(QStringLiteral("nr4"), QStringLiteral("NR4"));
        if (QTest::currentTestFailed()) { return; }
#ifdef HAVE_MNR
        checkPopupAgainstCatalogue(QStringLiteral("mnr"), QStringLiteral("MNR"));
#endif
    }

    // DFNR's quick controls open only where DFNR runs; the table they read
    // is checked here without them.
    void dfnrIsTheTable()
    {
        const QJsonArray dfnr = controlsOf(QStringLiteral("dfnr"));
        QCOMPARE(dfnr.size(), 2);
        const QJsonObject atten = dfnr.at(0).toObject();
        QCOMPARE(atten.value(QStringLiteral("label")).toString(),
                 QStringLiteral("Attenuation Limit"));
        QCOMPARE(atten.value(QStringLiteral("min")).toInt(), 0);
        QCOMPARE(atten.value(QStringLiteral("max")).toInt(), 100);
        QCOMPARE(atten.value(QStringLiteral("suffix")).toString(), QStringLiteral(" dB"));
        QCOMPARE(atten.value(QStringLiteral("default")).toDouble(), 100.0);
        QCOMPARE(atten.value(QStringLiteral("reset")).toInt(), 100);
        const QJsonObject beta = dfnr.at(1).toObject();
        QCOMPARE(beta.value(QStringLiteral("scale")).toDouble(), 0.01);
        QCOMPARE(beta.value(QStringLiteral("divide")).toInt(), 100);
        QCOMPARE(beta.value(QStringLiteral("decimals")).toInt(), 2);
        QCOMPARE(beta.value(QStringLiteral("reset")).toInt(), 0);
    }

    // NnrControls' spinboxes and combos are the catalogue's NNR controls.
    void nnrMatchesNnrControls()
    {
        RadioModel model;
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice);
        NnrControls controls(&model, slice, NnrControls::Presentation::Full);
        const QJsonArray nnr = controlsOf(QStringLiteral("nnr"));
        const QMap<QString, QString> spinFor{
            {QStringLiteral("nnrMaskFloorDb"), QStringLiteral("nnrMaskFloorSpin")},
            {QStringLiteral("nnrAlpha"), QStringLiteral("nnrAlphaSpin")},
            {QStringLiteral("nnrAlphaKneeDb"), QStringLiteral("nnrAlphaKneeSpin")},
            {QStringLiteral("nnrTauSeconds"), QStringLiteral("nnrTauSpin")},
            {QStringLiteral("nnrMaxGainDb"), QStringLiteral("nnrMaxGainSpin")},
            {QStringLiteral("nnrAttackMs"), QStringLiteral("nnrAttackSpin")},
            {QStringLiteral("nnrReleaseMs"), QStringLiteral("nnrReleaseSpin")},
        };
        const QMap<QString, QString> comboFor{
            {QStringLiteral("nnrModelSlot"), QStringLiteral("nnrModelCombo")},
            {QStringLiteral("nnrPosition"), QStringLiteral("nnrPositionCombo")},
        };
        for (const QJsonValue& value : nnr) {
            const QJsonObject control = value.toObject();
            const QString property = control.value(QStringLiteral("property")).toString();
            if (control.value(QStringLiteral("kind")).toString() == QStringLiteral("slider")) {
                auto* spin = controls.findChild<QDoubleSpinBox*>(spinFor.value(property));
                QVERIFY2(spin, qPrintable(property));
                QCOMPARE(spin->minimum(), control.value(QStringLiteral("min")).toDouble());
                QCOMPARE(spin->maximum(), control.value(QStringLiteral("max")).toDouble());
                QCOMPARE(spin->singleStep(), control.value(QStringLiteral("step")).toDouble());
                QCOMPARE(spin->decimals(), control.value(QStringLiteral("decimals")).toInt());
                QCOMPARE(spin->suffix(), control.value(QStringLiteral("suffix")).toString());
                QCOMPARE(spin->value(), control.value(QStringLiteral("default")).toDouble());
            } else {
                auto* combo = controls.findChild<QComboBox*>(comboFor.value(property));
                QVERIFY2(combo, qPrintable(property));
                const QJsonArray options = control.value(QStringLiteral("options")).toArray();
                QCOMPARE(combo->count(), options.size());
                for (int i = 0; i < combo->count(); ++i) {
                    QCOMPARE(combo->itemText(i),
                             options.at(i).toObject().value(QStringLiteral("label")).toString());
                    QCOMPARE(combo->itemData(i).toInt(),
                             options.at(i).toObject().value(QStringLiteral("id")).toInt());
                }
            }
        }
        // "Reset tuning" restores the defaults and keeps the model.
        slice->setNnrModelSlot(1);
        slice->setNnrAlpha(3.0);
        slice->setNnrTauSeconds(20.0);
        slice->resetNnrTuning();
        QCOMPARE(slice->nnrModelSlot(), 1);
        QCOMPARE(slice->nnrAlpha(), ControlRanges::kNnrAlpha.reset);
        QCOMPARE(slice->nnrTauSeconds(), ControlRanges::kNnrTau.reset);
        QVERIFY(!ControlRanges::kNnrModel.hasReset);
    }
};

QTEST_MAIN(TstCatalogueRanges)
#include "tst_catalogue_ranges.moc"
