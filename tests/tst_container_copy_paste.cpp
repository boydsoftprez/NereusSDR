// no-port-check: NereusSDR-original offscreen test of the item editor's
// copy and paste settings rule (ContainerSettingsDialog::canPasteSettings).
// Modification history (NereusSDR):
//   2026-10-26 - Settings paste between compatible bar meters and keep the
//                target's reading, units and average marker; other types
//                paste only onto the same type. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Final review fix wave (I4): a same-type paste keeps a web
//                image's and an LED's own guid and 4-char ID. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - PR 677 review fix wave (I1, I2): a same-type paste keeps
//                what Thetis keeps (units, button grids' buttons) and the
//                target's reading, layer and trace 2 source; picked
//                colours arrive with the settings. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.

#include <QtTest/QtTest>
#include <QJsonArray>
#include <QJsonObject>

#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/meters/MeterPoller.h"  // MeterBinding
#include "gui/meters/AntennaButtonItem.h"
#include "gui/meters/LEDItem.h"
#include "gui/meters/OtherButtonItem.h"
#include "gui/meters/TuneStepButtonItem.h"
#include "gui/meters/presets/HistoryGraphPresetItem.h"
#include "gui/meters/WebImageItem.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/meters/presets/SMeterPresetItem.h"

using namespace NereusSDR;

class TstContainerCopyPaste : public QObject
{
    Q_OBJECT

private slots:
    void compatibleBarsPaste();
    void selfContainedTypesPasteOnlyOntoTheirOwnType();
    void nereusOnlyTypesPasteOnlyOntoTheirOwnType();
    void itemTypeIdNamesTheBarFlavor();
    void alcOntoMicKeepsReadingAndUnits();
    void sourceWithoutAverageMarkerKeepsTargetsMarker();
    void refusedPasteLeavesTargetUnchanged();
    void sameTypePasteKeepsUnitsAndReading();
    void pasteKeepsTheTargetsFourCharId();
    void otherButtonsKeepTheirButtons();
    void tuneStepKeepsItsSteps();
    void antennaKeepsItsButtons();
    void pasteKeepsReadingAndLayer();
    void pickedColoursArriveWithTheSettings();
};

void TstContainerCopyPaste::compatibleBarsPaste()
{
    QVERIFY(ContainerSettingsDialog::canPasteSettings(QStringLiteral("meter.alc"), QStringLiteral("meter.mic")));
    QVERIFY(ContainerSettingsDialog::canPasteSettings(QStringLiteral("meter.mic"), QStringLiteral("meter.alc")));
    QVERIFY(ContainerSettingsDialog::canPasteSettings(QStringLiteral("meter.comp"), QStringLiteral("meter.signalAvg")));
    QVERIFY(ContainerSettingsDialog::canPasteSettings(QStringLiteral("meter.pwr"), QStringLiteral("meter.swr")));
    QVERIFY(ContainerSettingsDialog::canPasteSettings(QStringLiteral("meter.adcMax"), QStringLiteral("meter.cfcGain")));
    QVERIFY(!ContainerSettingsDialog::canPasteSettings(QString(), QStringLiteral("meter.mic")));
    QVERIFY(!ContainerSettingsDialog::canPasteSettings(QStringLiteral("meter.mic"), QString()));
}

void TstContainerCopyPaste::selfContainedTypesPasteOnlyOntoTheirOwnType()
{
    QVERIFY(!ContainerSettingsDialog::canPasteSettings(QStringLiteral("meter.magicEye"), QStringLiteral("meter.signalText")));
    QVERIFY(!ContainerSettingsDialog::canPasteSettings(QStringLiteral("meter.customBar"), QStringLiteral("meter.mic")));
    QVERIFY(!ContainerSettingsDialog::canPasteSettings(QStringLiteral("meter.mic"), QStringLiteral("meter.customBar")));
    QVERIFY(!ContainerSettingsDialog::canPasteSettings(QStringLiteral("meter.alc"), QStringLiteral("meter.magicEye")));
    // Thetis tests the target, not the source, for ROTATOR and HISTORY in
    // its second list; NereusSDR refuses them in either direction.
    QVERIFY(!ContainerSettingsDialog::canPasteSettings(QStringLiteral("ROTATOR"), QStringLiteral("meter.mic")));
    QVERIFY(!ContainerSettingsDialog::canPasteSettings(QStringLiteral("meter.historyGraph"), QStringLiteral("meter.mic")));
    QVERIFY(!ContainerSettingsDialog::canPasteSettings(QStringLiteral("LED"), QStringLiteral("BAR")));
    QVERIFY(ContainerSettingsDialog::canPasteSettings(QStringLiteral("meter.magicEye"), QStringLiteral("meter.magicEye")));
    QVERIFY(ContainerSettingsDialog::canPasteSettings(QStringLiteral("meter.customBar"), QStringLiteral("meter.customBar")));
    QVERIFY(ContainerSettingsDialog::canPasteSettings(QStringLiteral("LED"), QStringLiteral("LED")));
}

void TstContainerCopyPaste::nereusOnlyTypesPasteOnlyOntoTheirOwnType()
{
    QVERIFY(!ContainerSettingsDialog::canPasteSettings(QStringLiteral("meter.radeMicWaveform"), QStringLiteral("meter.mic")));
    QVERIFY(!ContainerSettingsDialog::canPasteSettings(QStringLiteral("meter.radeMicWaveform"), QStringLiteral("meter.radeSpeechWaveform")));
    QVERIFY(!ContainerSettingsDialog::canPasteSettings(QStringLiteral("meter.contest"), QStringLiteral("meter.clock")));
    QVERIFY(ContainerSettingsDialog::canPasteSettings(QStringLiteral("meter.contest"), QStringLiteral("meter.contest")));
}

void TstContainerCopyPaste::itemTypeIdNamesTheBarFlavor()
{
    BarPresetItem alc; alc.configureAsAlc();
    QCOMPARE(ContainerSettingsDialog::pasteTypeId(alc), QStringLiteral("meter.alc"));
    BarPresetItem custom; custom.configureAsCustom(MeterBinding::TxMic, 0, 10, QStringLiteral("Title"));
    QCOMPARE(ContainerSettingsDialog::pasteTypeId(custom), QStringLiteral("meter.customBar"));
    SMeterPresetItem sMeter;
    QCOMPARE(ContainerSettingsDialog::pasteTypeId(sMeter), QStringLiteral("meter.signalAvg"));
}

void TstContainerCopyPaste::alcOntoMicKeepsReadingAndUnits()
{
    BarPresetItem source; source.configureAsAlc();
    QVERIFY(source.applyConfiguration({{"style", "Segments"}, {"barColor", "#ff00ff00"}, {"attack", 0.25},
        {"units", "dBx"}, {"subColor", "#ff0000ff"}, {"showSub", false}, {"label", "Source title"}}));
    BarPresetItem target; target.configureAsMic();
    target.setRect(0.1f, 0.4f, 0.8f, 0.2f);
    const QJsonObject before = target.configuration();

    QVERIFY(ContainerSettingsDialog::pasteItemSettings(source.serialize(), QStringLiteral("meter.alc"), target));

    const QJsonObject after = target.configuration();
    QCOMPARE(target.typeId(), QStringLiteral("meter.mic"));
    QCOMPARE(target.bindingId(), int(MeterBinding::TxMicPeak));
    QCOMPARE(after.value("secondaryBindingId").toInt(), int(MeterBinding::TxMic));
    QCOMPARE(after.value("units"), before.value("units"));
    QCOMPARE(after.value("label"), before.value("label"));
    QCOMPARE(after.value("minValue"), before.value("minValue"));
    QCOMPARE(after.value("maxValue"), before.value("maxValue"));
    QCOMPARE(after.value("y"), before.value("y"));
    QCOMPARE(after.value("h"), before.value("h"));
    // The source's appearance and dynamics arrive.
    QCOMPARE(after.value("style").toString(), QStringLiteral("Segments"));
    QCOMPARE(after.value("barColor").toString(), QStringLiteral("#ff00ff00"));
    QCOMPARE(after.value("attack").toDouble(), 0.25);
    // The source has an average marker, so its marker settings arrive too.
    QCOMPARE(after.value("subColor").toString(), QStringLiteral("#ff0000ff"));
    QCOMPARE(after.value("showSub").toBool(), false);
}

void TstContainerCopyPaste::sourceWithoutAverageMarkerKeepsTargetsMarker()
{
    BarPresetItem source; QVERIFY(source.configureVariant(QStringLiteral("AgcGain")));
    QCOMPARE(source.configuration().value("secondaryBindingId").toInt(), -1);
    QVERIFY(source.applyConfiguration({{"subColor", "#ff0000ff"}, {"showSub", false}, {"style", "Solid"}}));
    BarPresetItem target; target.configureAsMic();
    QVERIFY(target.applyConfiguration({{"subColor", "#ff123456"}, {"showSub", true}}));

    QVERIFY(ContainerSettingsDialog::pasteItemSettings(source.serialize(), QStringLiteral("meter.agcGain"), target));

    const QJsonObject after = target.configuration();
    QCOMPARE(after.value("style").toString(), QStringLiteral("Solid"));
    QCOMPARE(after.value("subColor").toString(), QStringLiteral("#ff123456"));
    QCOMPARE(after.value("showSub").toBool(), true);
    QCOMPARE(after.value("secondaryBindingId").toInt(), int(MeterBinding::TxMic));
}

void TstContainerCopyPaste::refusedPasteLeavesTargetUnchanged()
{
    BarPresetItem source; source.configureAsCustom(MeterBinding::TxMic, 0, 10, QStringLiteral("Title"));
    QVERIFY(source.applyConfiguration({{"style", "Segments"}}));
    BarPresetItem target; target.configureAsMic();
    const QString before = target.serialize();

    QVERIFY(!ContainerSettingsDialog::pasteItemSettings(source.serialize(), QStringLiteral("meter.customBar"), target));
    QCOMPARE(target.serialize(), before);
}

// Thetis btnMeterPasteSettings_Click keeps the target's unit on every
// paste (ContainerSettingsDialog.cpp cites it); the reading and place stay
// too. A same-type
// paste brings the title and scale (they are the source's settings).
void TstContainerCopyPaste::sameTypePasteKeepsUnitsAndReading()
{
    BarPresetItem source; source.configureAsCustom(MeterBinding::TxMic, 0, 10, QStringLiteral("Title"));
    QVERIFY(source.applyConfiguration({{"style", "Segments"}, {"units", "V"}}));
    BarPresetItem target; target.configureAsCustom(MeterBinding::TxAlc, -5, 5, QStringLiteral("Other"));
    QVERIFY(target.applyConfiguration({{"units", "dB"}}));
    const QJsonObject before = target.configuration();

    QVERIFY(ContainerSettingsDialog::pasteItemSettings(source.serialize(), QStringLiteral("meter.customBar"), target));
    const QJsonObject after = target.configuration();
    QCOMPARE(after.value("style").toString(), QStringLiteral("Segments"));
    QCOMPARE(after.value("label").toString(), QStringLiteral("Title"));
    QCOMPARE(after.value("units"), before.value("units"));
    QCOMPARE(after.value("bindingId"), before.value("bindingId"));
    QCOMPARE(after.value("secondaryBindingId"), before.value("secondaryBindingId"));
    QCOMPARE(target.bindingId(), before.value("bindingId").toInt());
}

// Fix wave I4: a web image and an LED keep their own guid and 4-char ID
// across a same-type paste; the other settings come from the source.
void TstContainerCopyPaste::pasteKeepsTheTargetsFourCharId()
{
    WebImageItem webSource; webSource.setGuid(QUuid::createUuid());
    WebImageItem webTarget; webTarget.setGuid(QUuid::createUuid());
    const QUuid webGuid = webTarget.guid();
    const QString webId = webTarget.fourChar();
    QVERIFY(webId != webSource.fourChar());
    QVERIFY(ContainerSettingsDialog::pasteItemSettings(
        webSource.serialize(), ContainerSettingsDialog::pasteTypeId(webSource), webTarget));
    QCOMPARE(webTarget.guid(), webGuid);
    QCOMPARE(webTarget.fourChar(), webId);

    LEDItem ledSource; ledSource.setGuid(QUuid::createUuid());
    LEDItem ledTarget; ledTarget.setGuid(QUuid::createUuid());
    const QUuid ledGuid = ledTarget.guid();
    QVERIFY(ContainerSettingsDialog::pasteItemSettings(
        ledSource.serialize(), ContainerSettingsDialog::pasteTypeId(ledSource), ledTarget));
    QCOMPARE(ledTarget.guid(), ledGuid);
}

// From Thetis setup.cs:27047-27059 [v2.10.3.15]: an other-buttons grid
// keeps its button order, visible buttons and macros.
void TstContainerCopyPaste::otherButtonsKeepTheirButtons()
{
    using Id = OtherButtonItem::ButtonId;
    OtherButtonItem source;
    source.setOnlyVisible(Id::Anf);
    source.macroSettings(0).onText = QStringLiteral("Source on");
    source.setColumns(3);
    OtherButtonItem target;
    target.setOnlyVisible(Id::Xpa);
    target.setButtonVisible(Id::Macro0, true);
    target.macroSettings(0).onText = QStringLiteral("Target on");
    QVector<short> order = target.buttonMap();
    std::swap(order[0], order[1]);
    target.setButtonMap(order);
    target.setColumns(6);
    const QVector<qint32> groups = target.visibleGroups();

    QVERIFY(ContainerSettingsDialog::pasteItemSettings(
        source.serialize(), ContainerSettingsDialog::pasteTypeId(source), target));
    QCOMPARE(target.columns(), 3);
    QCOMPARE(target.visibleGroups(), groups);
    QCOMPARE(target.buttonMap(), order);
    QCOMPARE(target.macroSettings(0).onText, QStringLiteral("Target on"));
}

// From Thetis setup.cs:27060-27064 [v2.10.3.15]: a tune step grid keeps
// its visible steps; the lit step is the radio's.
void TstContainerCopyPaste::tuneStepKeepsItsSteps()
{
    TuneStepButtonItem source;
    source.setVisibleBits(0x3u);
    source.setActiveStep(1);
    source.setColumns(5);
    TuneStepButtonItem target;
    target.setVisibleBits(0xF0u);
    target.setActiveStep(4);
    target.setColumns(2);
    const uint32_t steps = target.visibleBits();
    QVERIFY(steps != source.visibleBits());

    QVERIFY(ContainerSettingsDialog::pasteItemSettings(
        source.serialize(), ContainerSettingsDialog::pasteTypeId(source), target));
    QCOMPARE(target.columns(), 5);
    QCOMPARE(target.visibleBits(), steps);
    QCOMPARE(target.activeStep(), 4);
}

// From Thetis setup.cs:27065-27079 [v2.10.3.15]: an antenna grid keeps
// which antenna buttons it shows.
void TstContainerCopyPaste::antennaKeepsItsButtons()
{
    AntennaButtonItem source;
    source.setVisibleBits(0x3u);
    source.setColumns(2);
    AntennaButtonItem target;
    target.setVisibleBits(0x1Cu);
    target.setColumns(1);
    const uint32_t shown = target.visibleBits();
    QVERIFY(shown != source.visibleBits());

    QVERIFY(ContainerSettingsDialog::pasteItemSettings(
        source.serialize(), ContainerSettingsDialog::pasteTypeId(source), target));
    QCOMPARE(target.columns(), 2);
    QCOMPARE(target.visibleBits(), shown);
}

// The target keeps its reading and its layer, and a Signal history face
// its trace 2 source; the source's look arrives.
void TstContainerCopyPaste::pasteKeepsReadingAndLayer()
{
    LEDItem ledSource; ledSource.setBindingId(5); ledSource.setZOrder(9);
    ledSource.setTrueColour(QColor(0x11, 0x22, 0x33));
    LEDItem ledTarget; ledTarget.setBindingId(7); ledTarget.setZOrder(2);
    QVERIFY(ContainerSettingsDialog::pasteItemSettings(
        ledSource.serialize(), ContainerSettingsDialog::pasteTypeId(ledSource), ledTarget));
    QCOMPARE(ledTarget.bindingId(), 7);
    QCOMPARE(ledTarget.zOrder(), 2);
    QCOMPARE(ledTarget.trueColour(), QColor(0x11, 0x22, 0x33));

    HistoryGraphPresetItem faceSource;
    QVERIFY(faceSource.applyConfiguration({{"trace2Context", QJsonObject{{"sliceId", 1}}}, {"historyMs", 2000}}));
    faceSource.setZOrder(8);
    HistoryGraphPresetItem faceTarget;
    QJsonArray channels = faceTarget.configuration().value("channels").toArray();
    QJsonObject first = channels[0].toObject();
    first["bindingId"] = int(MeterBinding::TxMic);
    channels[0] = first;
    QVERIFY(faceTarget.applyConfiguration({{"channels", channels}}));
    faceTarget.setZOrder(3);
    const int ownBinding = faceTarget.configuration().value("bindingId").toInt();
    QCOMPARE(ownBinding, int(MeterBinding::TxMic));

    QVERIFY(ContainerSettingsDialog::pasteItemSettings(
        faceSource.serialize(), ContainerSettingsDialog::pasteTypeId(faceSource), faceTarget));
    const QJsonObject after = faceTarget.configuration();
    QCOMPARE(after.value("historyMs").toInt(), 2000);
    QCOMPARE(after.value("bindingId").toInt(), ownBinding);
    QCOMPARE(after.value("channels").toArray()[0].toObject().value("bindingId").toInt(), ownBinding);
    QVERIFY(!after.contains("trace2Context"));
    QCOMPARE(faceTarget.zOrder(), 3);
}

// Colours picked on the source are picked on the target after the paste;
// a bar that keeps its own sub marker keeps its own pick for it.
void TstContainerCopyPaste::pickedColoursArriveWithTheSettings()
{
    LEDItem source;
    LEDItem target;
    target.setColourUserSet(QStringLiteral("Led.falseColour"), true);
    QVERIFY(ContainerSettingsDialog::pasteItemSettings(source.serialize(), ContainerSettingsDialog::pasteTypeId(source),
                                                       target, {QStringLiteral("Led.trueColour")}));
    QCOMPARE(target.userColourFields(), QStringList{QStringLiteral("Led.trueColour")});

    BarPresetItem bar; QVERIFY(bar.configureVariant(QStringLiteral("AgcGain")));
    BarPresetItem mic; mic.configureAsMic();
    mic.setColourUserSet(QStringLiteral("BarPreset.subColor"), true);
    QVERIFY(ContainerSettingsDialog::pasteItemSettings(bar.serialize(), QStringLiteral("meter.agcGain"), mic,
                                                       {QStringLiteral("BarPreset.barColor")}));
    QVERIFY(mic.colourUserSet(QStringLiteral("BarPreset.barColor")));
    QVERIFY(mic.colourUserSet(QStringLiteral("BarPreset.subColor")));
}

QTEST_MAIN(TstContainerCopyPaste)
#include "tst_container_copy_paste.moc"
