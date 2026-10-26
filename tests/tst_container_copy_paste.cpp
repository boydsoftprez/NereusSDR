// no-port-check: NereusSDR-original offscreen test of the item editor's
// copy and paste settings rule (ContainerSettingsDialog::canPasteSettings).
// Modification history (NereusSDR):
//   2026-10-26 - Settings paste between compatible bar meters and keep the
//                target's reading, units and average marker; other types
//                paste only onto the same type. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.

#include <QtTest/QtTest>
#include <QJsonObject>

#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/meters/MeterPoller.h"  // MeterBinding
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
    void sameTypePasteCopiesTheWholeRecord();
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

void TstContainerCopyPaste::sameTypePasteCopiesTheWholeRecord()
{
    BarPresetItem source; source.configureAsCustom(MeterBinding::TxMic, 0, 10, QStringLiteral("Title"));
    QVERIFY(source.applyConfiguration({{"style", "Segments"}, {"units", "V"}}));
    BarPresetItem target; target.configureAsCustom(MeterBinding::TxAlc, -5, 5, QStringLiteral("Other"));

    QVERIFY(ContainerSettingsDialog::pasteItemSettings(source.serialize(), QStringLiteral("meter.customBar"), target));
    QCOMPARE(target.serialize(), source.serialize());
}

QTEST_MAIN(TstContainerCopyPaste)
#include "tst_container_copy_paste.moc"
