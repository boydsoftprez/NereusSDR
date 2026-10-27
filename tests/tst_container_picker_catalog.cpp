// no-port-check: NereusSDR-original tests of the container picker's names
// and headings.
// Modification history (NereusSDR):
//   2026-10-26 - Written: every type's picker title and heading, the
//                 grouped Available list, and old default names taking the
//                 new labels on load. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
#include <QtTest>
#include <QHash>
#include <QListWidget>
#include <QPushButton>
#include <QSet>
#include <QSplitter>
#include <QTemporaryDir>

#include <iterator>

#include "core/AppSettings.h"
#include "gui/applets/RxApplet.h"
#include "gui/applets/RxSectionContent.h"
#include "gui/applets/TxApplet.h"
#include "gui/applets/TxSectionContent.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerDocumentCodec.h"
#include "gui/containers/ContainerEditSession.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerSettingsDialog.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/ContentEntryMigration.h"
#include "gui/instruments/LongpathContent.h"
#include "gui/instruments/SMeterDialContent.h"
#include "gui/meters/MeterPoller.h"
#include "gui/styles/WorkspacePreview.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

struct Expected {
    const char* typeId;
    const char* title;
    ContentGroup group;
};

constexpr ContentGroup kMeters = ContentGroup::ReadyMadeMeters;
constexpr ContentGroup kBlocks = ContentGroup::BuildingBlocks;
constexpr ContentGroup kButtons = ContentGroup::Buttons;
constexpr ContentGroup kPanels = ContentGroup::Panels;
constexpr ContentGroup kSections = ContentGroup::PanelSections;
constexpr ContentGroup kLongpath = ContentGroup::LongpathInstruments;

// Every type the picker offers with workspace preview on and every widget
// type registered: its title and heading.
const Expected kExpected[] = {
    {"meter.radeMicWaveform", "Microphone waveform", kMeters},
    {"meter.radeSpeechWaveform", "Decoded speech waveform", kMeters},
    {"meter.powerSwr", "Power / SWR", kMeters},
    {"meter.ananMulti", "ANAN multi meter", kMeters},
    {"meter.crossNeedle", "Cross needle", kMeters},
    {"meter.magicEye", "Magic eye", kMeters},
    {"meter.signalText", "Signal text", kMeters},
    {"meter.historyGraph", "Signal history", kMeters},
    {"meter.vfoDisplay", "VFO display", kMeters},
    {"meter.clock", "Clock", kMeters},
    {"meter.contest", "Contest controls", kMeters},
    {"meter.sMeter", "S-Meter bar", kMeters},
    {"meter.spacer", "Spacer", kMeters},
    {"meter.comp", "Compression bar", kMeters},
    {"meter.eq", "EQ bar", kMeters},
    {"meter.leveler", "Leveler bar", kMeters},
    {"meter.cfc", "CFC bar", kMeters},
    {"meter.cfcGain", "CFC gain bar", kMeters},
    {"meter.levelerGain", "Leveler gain bar", kMeters},
    {"meter.alcGain", "ALC gain bar", kMeters},
    {"meter.alcGroup", "ALC group bar", kMeters},
    {"meter.agc", "AGC bar", kMeters},
    {"meter.agcGain", "AGC gain bar", kMeters},
    {"meter.signal", "Signal peak bar", kMeters},
    {"meter.signalAvg", "Signal average bar", kMeters},
    {"meter.signalMaxBin", "Signal max bin bar", kMeters},
    {"meter.adc", "ADC bar", kMeters},
    {"meter.adcMax", "ADC maximum bar", kMeters},
    {"meter.pbSnr", "PB SNR bar", kMeters},
    {"meter.pwr", "Power bar", kMeters},
    {"meter.reversePwr", "Reverse power bar", kMeters},
    {"meter.swr", "SWR bar", kMeters},
    {"meter.mic", "Mic bar", kMeters},
    {"meter.alc", "ALC bar", kMeters},
    {"meter.customBar", "Custom bar face", kMeters},
    {"sMeter.dial", "S-Meter dial", kMeters},
    {"BAR", "Bar", kBlocks},
    {"SOLID", "Solid color", kBlocks},
    {"IMAGE", "Image", kBlocks},
    {"SCALE", "Scale", kBlocks},
    {"TEXT", "Text", kBlocks},
    {"NEEDLE", "Needle", kBlocks},
    {"SPACER", "Spacer", kBlocks},
    {"FADECOVER", "Fade cover", kBlocks},
    {"LED", "LED", kBlocks},
    {"HISTORY", "History graph", kBlocks},
    {"MAGICEYE", "Magic eye", kBlocks},
    {"NEEDLESCALEPWR", "Power scale", kBlocks},
    {"SIGNALTEXT", "Signal text", kBlocks},
    {"DIAL", "Dial", kBlocks},
    {"TEXTOVERLAY", "Text overlay", kBlocks},
    {"WEBIMAGE", "Web image", kBlocks},
    {"FILTERDISPLAY", "Filter display", kBlocks},
    {"ROTATOR", "Rotator", kBlocks},
    {"VFO", "VFO display", kBlocks},
    {"CLOCK", "Clock", kBlocks},
    {"CLICKBOX", "Click box", kBlocks},
    {"DATAOUT", "Data output", kBlocks},
    {"BANDBTNS", "Band buttons", kButtons},
    {"NEREUSBAND", "Band buttons (panel style)", kButtons},
    {"MODEBTNS", "Mode buttons", kButtons},
    {"FILTERBTNS", "Filter buttons", kButtons},
    {"ANTENNABTNS", "Antenna buttons", kButtons},
    {"TUNESTEPBTNS", "Tune step buttons", kButtons},
    {"OTHERBTNS", "Other buttons", kButtons},
    {"control.mox", "MOX", kButtons},
    {"control.tune", "Tune", kButtons},
    {"control.monitor", "Monitor", kButtons},
    {"control.twoTone", "2Tone", kButtons},
    {"control.pureSignal", "PureSignal", kButtons},
    {"control.anf", "ANF", kButtons},
    {"control.snb", "SNB", kButtons},
    {"control.mnf", "MNF", kButtons},
    {"control.peak", "Peak", kButtons},
    {"control.ctun", "CTUN", kButtons},
    {"control.vax1", "VAX 1", kButtons},
    {"control.vax2", "VAX 2", kButtons},
    {"control.mute", "Mute", kButtons},
    {"control.binaural", "Binaural", kButtons},
    {"control.duplex", "Duplex", kButtons},
    {"VOICERECPLAY", "Voice record and play", kButtons},
    {"DISCORDBTNS", "Discord control (removed)", kButtons},
    {"applet:rx", "RX", kPanels},
    {"applet:Display", "Display", kPanels},
    {"applet:TX", "TX", kPanels},
    {"applet:PHCW", "Phone / CW", kPanels},
    {"applet:RADE", "RADE", kPanels},
    {"applet:vax", "VAX", kPanels},
    {"applet:pure_signal", "PureSignal", kPanels},
    {"applet:mod_monitor", "AM Mod Monitor", kPanels},
    {"applet:cat", "CAT", kPanels},
    {"applet:tci", "TCI Server", kPanels},
    {"applet:tci_clients", "TCI Clients", kPanels},
    {"applet:amp", "4O3A Power Genius", kPanels},
    {"applet:tuner", "4O3A Tuner Genius", kPanels},
    {"applet:rotor", "Rotor", kPanels},
    {"applet:RfKit", "RF-Kit RF2K-S", kPanels},
    {"applet:s_meter", "S-Meter", kPanels},
    {"tx.monitor", "TX Monitor", kSections},
    {"tx.power", "TX Power", kSections},
    {"tx.filter", "TX Filter", kSections},
    {"tx.chain", "TX Chain", kSections},
    {"tx.vox", "TX VOX", kSections},
    {"tx.processing", "TX Processing", kSections},
    {"tx.keying", "TX Keying", kSections},
    {"rx.antennas", "RX Antennas", kSections},
    {"rx.mode", "RX Mode", kSections},
    {"rx.step", "RX Step", kSections},
    {"rx.filter", "RX Filter", kSections},
    {"rx.pan", "RX Pan", kSections},
    {"rx.squelch", "RX Squelch", kSections},
    {"rx.attenuator", "RX Attenuator", kSections},
    {"rx.agc", "RX AGC", kSections},
    {"rx.rit", "RX RIT", kSections},
    {"rx.xit", "RX XIT", kSections},
    {"longpath.frequency", "Frequency", kLongpath},
    {"longpath.frequencyCustom", "Custom frequency", kLongpath},
    {"longpath.needle", "Needle meter", kLongpath},
    {"longpath.bar", "Bar meter", kLongpath},
    {"longpath.filter", "Filter passband", kLongpath},
};

struct Rename {
    const char* typeId;
    const char* oldLabel;
    const char* newLabel;
};

// Every label this change renamed: its typeId, the old default and the new.
const Rename kRenames[] = {
    {"meter.sMeter", "S-meter bar (additional)", "S-Meter bar"},
    {"meter.comp", "Comp bar", "Compression bar"},
    {"meter.eq", "Eq bar", "EQ bar"},
    {"meter.cfc", "Cfc bar", "CFC bar"},
    {"meter.cfcGain", "CfcGain bar", "CFC gain bar"},
    {"meter.levelerGain", "LevelerGain bar", "Leveler gain bar"},
    {"meter.alcGain", "AlcGain bar", "ALC gain bar"},
    {"meter.alcGroup", "AlcGroup bar", "ALC group bar"},
    {"meter.agc", "Agc bar", "AGC bar"},
    {"meter.agcGain", "AgcGain bar", "AGC gain bar"},
    {"meter.signal", "Signal bar", "Signal peak bar"},
    {"meter.signalAvg", "SignalAvg bar", "Signal average bar"},
    {"meter.signalMaxBin", "SignalMaxBin bar", "Signal max bin bar"},
    {"meter.adc", "Adc bar", "ADC bar"},
    {"meter.adcMax", "AdcMax bar", "ADC maximum bar"},
    {"meter.pbSnr", "PbSnr bar", "PB SNR bar"},
    {"meter.mic", "Mic", "Mic bar"},
    {"meter.alc", "ALC", "ALC bar"},
    {"sMeter.dial", "S-Meter dial (additional)", "S-Meter dial"},
    {"SOLID", "SolidColour", "Solid color"},
    {"FADECOVER", "FadeCover", "Fade cover"},
    {"HISTORY", "HistoryGraph", "History graph"},
    {"MAGICEYE", "MagicEye", "Magic eye"},
    {"NEEDLESCALEPWR", "NeedleScalePwr", "Power scale"},
    {"SIGNALTEXT", "SignalText", "Signal text"},
    {"TEXTOVERLAY", "TextOverlay", "Text overlay"},
    {"WEBIMAGE", "WebImage", "Web image"},
    {"FILTERDISPLAY", "FilterDisplay", "Filter display"},
    {"VFO", "VfoDisplay", "VFO display"},
    {"CLICKBOX", "ClickBox", "Click box"},
    {"DATAOUT", "DataOut", "Data output"},
    {"BANDBTNS", "BandButton", "Band buttons"},
    {"NEREUSBAND", "NereusBandButton", "Band buttons (panel style)"},
    {"MODEBTNS", "ModeButton", "Mode buttons"},
    {"FILTERBTNS", "FilterButton", "Filter buttons"},
    {"ANTENNABTNS", "AntennaButton", "Antenna buttons"},
    {"TUNESTEPBTNS", "TuneStepButton", "Tune step buttons"},
    {"OTHERBTNS", "OtherButton", "Other buttons"},
    {"VOICERECPLAY", "VoiceRecordPlay", "Voice record and play"},
    {"applet:mod_monitor", "Mod Monitor", "AM Mod Monitor"},
    {"applet:tci", "TCI", "TCI Server"},
    {"applet:tci_clients", "Client Chain", "TCI Clients"},
    {"applet:amp", "Amplifier", "4O3A Power Genius"},
    {"applet:tuner", "Tuner", "4O3A Tuner Genius"},
    {"applet:RfKit", "RF Kit", "RF-Kit RF2K-S"},
    {"longpath.frequency", "Frequency (Longpath)", "Frequency"},
    {"longpath.frequencyCustom", "Frequency (Custom)", "Custom frequency"},
    {"longpath.needle", "Needle meter (Longpath)", "Needle meter"},
    {"longpath.bar", "Bar meter (Longpath)", "Bar meter"},
    {"longpath.filter", "Filter passband (Longpath)", "Filter passband"},
};

const QVector<ContentGroup> kHeadingOrder{kMeters, kBlocks, kButtons, kPanels, kSections, kLongpath};

// A registry with every widget type registered, as MainWindow does with
// workspace preview on.
struct FullRegistry {
    RadioModel radio;
    TxApplet txApplet{&radio};
    RxApplet rxApplet{nullptr, &radio};
    ContainerContentRegistry registry;
    FullRegistry()
    {
        registerTxSectionContent(registry, &txApplet);
        registerRxSectionContent(registry, &rxApplet);
        LongpathContent::registerAll(registry, {});
        SMeterDialContent::registerDial(registry, {});
    }
};

// One entry saved and loaded through the document codec.
ContentEntry roundTrip(const ContentEntry& entry)
{
    WorkspaceDocument doc;
    doc.mainContainerId = QStringLiteral("main");
    ContainerDocument container;
    container.id = QStringLiteral("main");
    container.contents = {entry};
    doc.containers = {container};
    const DocumentResult decoded = ContainerDocumentCodec::decode(ContainerDocumentCodec::encode(doc));
    if (!decoded.ok || decoded.document.containers.isEmpty()
        || decoded.document.containers.first().contents.isEmpty()) {
        qWarning() << "decode failed:" << decoded.error;
        return {};
    }
    return decoded.document.containers.first().contents.first();
}

ContentEntry savedEntry(const QString& typeId, const QString& name)
{
    ContentEntry entry;
    entry.id = QStringLiteral("saved-") + typeId;
    entry.typeId = typeId;
    entry.name = name;
    return entry;
}

} // namespace

class TstContainerPickerCatalog : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        WorkspacePreview::setEnabledForTest(true);
    }

    void groupTitlesAreTheSixHeadings()
    {
        QStringList titles;
        for (const ContentGroup group : kHeadingOrder) {
            titles << ContainerContentRegistry::groupTitle(group);
        }
        QCOMPARE(titles, (QStringList{QStringLiteral("Ready-made meters"), QStringLiteral("Building blocks"),
                                      QStringLiteral("Buttons"), QStringLiteral("Panels"),
                                      QStringLiteral("Panel sections"), QStringLiteral("Longpath instruments")}));
    }

    void everyTitleAndHeadingMatchesTheTable()
    {
        FullRegistry full;
        QHash<QString, const Expected*> expected;
        for (const Expected& row : kExpected) {
            QVERIFY2(!expected.contains(QString::fromLatin1(row.typeId)), row.typeId);
            expected.insert(QString::fromLatin1(row.typeId), &row);
        }
        QSet<QString> seen;
        QHash<int, QSet<QString>> titlesByGroup;
        for (const ContentDescriptor& d : full.registry.descriptors()) {
            QVERIFY2(expected.contains(d.typeId), qPrintable(QStringLiteral("not in the table: ") + d.typeId));
            const Expected* row = expected.value(d.typeId);
            QCOMPARE(d.title, QString::fromUtf8(row->title));
            QVERIFY2(d.group == row->group, qPrintable(d.typeId));
            seen.insert(d.typeId);
            for (const char* banned : {"(Longpath)", "(Custom)", "(additional)", "VFO A", "VFO B", "RX1", "RX2"}) {
                QVERIFY2(!d.title.contains(QLatin1String(banned)), qPrintable(d.typeId + QStringLiteral(": ") + d.title));
            }
            QSet<QString>& titles = titlesByGroup[int(d.group)];
            QVERIFY2(!titles.contains(d.title), qPrintable(QStringLiteral("two \"") + d.title + QStringLiteral("\" under one heading")));
            titles.insert(d.title);
        }
        QCOMPARE(seen.size(), int(std::size(kExpected)));
    }

    void everyRenamedLabelIsTheRegistryTitle()
    {
        FullRegistry full;
        QHash<QString, QString> titles;
        for (const ContentDescriptor& d : full.registry.descriptors()) {
            titles.insert(d.typeId, d.title);
        }
        for (const Rename& rename : kRenames) {
            QVERIFY2(titles.contains(QString::fromLatin1(rename.typeId)), rename.typeId);
            QCOMPARE(titles.value(QString::fromLatin1(rename.typeId)), QString::fromUtf8(rename.newLabel));
            QVERIFY(full.registry.makeEntry(QString::fromLatin1(rename.typeId)).name == QString::fromUtf8(rename.newLabel));
        }
    }

    void anEntrySavedWithTheOldLabelLoadsWithTheNewOne()
    {
        const ContentEntry loaded = roundTrip(savedEntry(QStringLiteral("meter.comp"), QStringLiteral("Comp bar")));
        QCOMPARE(loaded.typeId, QStringLiteral("meter.comp"));
        QCOMPARE(loaded.name, QStringLiteral("Compression bar"));
        for (const Rename& rename : kRenames) {
            const ContentEntry each = roundTrip(savedEntry(QString::fromLatin1(rename.typeId),
                                                           QString::fromUtf8(rename.oldLabel)));
            QCOMPARE(each.typeId, QString::fromLatin1(rename.typeId));
            QCOMPARE(each.name, QString::fromUtf8(rename.newLabel));
        }
        // meter.sMeter's older typeId keeps its typeId and takes the name.
        const ContentEntry older = roundTrip(savedEntry(QStringLiteral("SMeterPreset"),
                                                        QStringLiteral("S-meter bar (additional)")));
        QCOMPARE(older.typeId, QStringLiteral("SMeterPreset"));
        QCOMPARE(older.name, QStringLiteral("S-Meter bar"));
    }

    void aNameTheOperatorChoseIsKept()
    {
        QCOMPARE(roundTrip(savedEntry(QStringLiteral("meter.comp"), QStringLiteral("My comp"))).name,
                 QStringLiteral("My comp"));
        QCOMPARE(roundTrip(savedEntry(QStringLiteral("applet:amp"), QStringLiteral("Shack amp"))).name,
                 QStringLiteral("Shack amp"));
        // Another type's old label is not this type's default.
        QCOMPARE(roundTrip(savedEntry(QStringLiteral("meter.eq"), QStringLiteral("Comp bar"))).name,
                 QStringLiteral("Comp bar"));
        // A control shortcut is saved as OTHERBTNS under its own name.
        QCOMPARE(roundTrip(savedEntry(QStringLiteral("OTHERBTNS"), QStringLiteral("MOX"))).name,
                 QStringLiteral("MOX"));
        // Not case-folded: only the exact old label is a default.
        QCOMPARE(roundTrip(savedEntry(QStringLiteral("meter.comp"), QStringLiteral("comp bar"))).name,
                 QStringLiteral("comp bar"));
    }

    void anEntryOnTheNewLabelIsUnchangedAndLoadingTwiceIsIdempotent()
    {
        const ContentEntry current = savedEntry(QStringLiteral("applet:tci"), QStringLiteral("TCI Server"));
        QCOMPARE(roundTrip(current), current);
        const ContentEntry once = roundTrip(savedEntry(QStringLiteral("longpath.needle"),
                                                       QStringLiteral("Needle meter (Longpath)")));
        const ContentEntry twice = roundTrip(once);
        QCOMPARE(once.name, QStringLiteral("Needle meter"));
        QCOMPARE(twice, once);
        ContentEntry direct = savedEntry(QStringLiteral("DATAOUT"), QStringLiteral("DataOut"));
        migrateEntryOnLoad(direct);
        migrateEntryOnLoad(direct);
        QCOMPARE(direct.name, QStringLiteral("Data output"));
        QCOMPARE(direct.typeId, QStringLiteral("DATAOUT"));
    }

    void theAvailableListShowsTheHeadingsAndAHeadingAddsNothing()
    {
        FullRegistry full;
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("settings")));
        ContainerWorkspaceStore store(settings);
        MeterPoller poller;
        QWidget root;
        QSplitter splitter(&root);
        ContainerManager manager(&root, &splitter);
        manager.setWorkspaceAdapter(&store, &full.registry);
        manager.setPreviewPoller(&poller);
        WorkspaceDocument document;
        document.mainContainerId = QStringLiteral("A");
        ContainerDocument container;
        container.id = QStringLiteral("A");
        container.layout = ContentLayout::VerticalStack;
        document.containers = {container};
        QCOMPARE(manager.commitWorkspace(document, 0).status, CommitStatus::Saved);

        ContainerSettingsDialog dialog(manager.container(QStringLiteral("A")), nullptr, &manager);
        QVERIFY(dialog.editSession());
        auto* available = dialog.findChild<QListWidget*>(QStringLiteral("containerAvailableContents"));
        QVERIFY(available);

        // Headings in order, each followed by its own entries in
        // descriptors() order.
        QStringList headings;
        QHash<QString, QStringList> rowsUnder;
        QString current;
        QVector<QListWidgetItem*> headingRows;
        for (int i = 0; i < available->count(); ++i) {
            QListWidgetItem* item = available->item(i);
            const QString typeId = item->data(Qt::UserRole).toString();
            if (typeId.isEmpty()) {
                QVERIFY(!item->data(Qt::UserRole).isValid());
                QCOMPARE(item->flags(), Qt::ItemFlags(Qt::NoItemFlags));
                QVERIFY(item->font().bold());
                current = item->text();
                headings << current;
                headingRows << item;
                continue;
            }
            QVERIFY2(!current.isEmpty(), qPrintable(typeId + QStringLiteral(" is above every heading")));
            rowsUnder[current] << typeId;
        }
        QStringList expectedHeadings;
        for (const ContentGroup group : kHeadingOrder) {
            expectedHeadings << ContainerContentRegistry::groupTitle(group);
        }
        QCOMPARE(headings, expectedHeadings);
        for (const ContentGroup group : kHeadingOrder) {
            QStringList inOrder;
            for (const ContentDescriptor& d : full.registry.descriptors()) {
                if (d.group == group) { inOrder << d.typeId; }
            }
            QCOMPARE(rowsUnder.value(ContainerContentRegistry::groupTitle(group)), inOrder);
        }

        // A singleton keeps its " — Move" suffix.
        for (int i = 0; i < available->count(); ++i) {
            if (available->item(i)->data(Qt::UserRole).toString() == QLatin1String("applet:amp")) {
                QCOMPARE(available->item(i)->text(), QStringLiteral("4O3A Power Genius ") + QChar(0x2014)
                                                         + QStringLiteral(" Move"));
            }
        }

        // A heading cannot be selected, and Add or double-click on it adds
        // nothing.
        QPushButton* add = nullptr;
        for (auto* button : dialog.findChildren<QPushButton*>()) {
            if (button->text() == QStringLiteral("Add →")) { add = button; }
        }
        QVERIFY(add);
        const auto before = dialog.editSession()->draft().containers[0].contents.size();
        for (QListWidgetItem* heading : headingRows) {
            available->clearSelection();
            available->setCurrentItem(heading);
            QVERIFY(!heading->isSelected());
            QVERIFY(available->selectedItems().isEmpty());
            add->click();
            emit available->itemDoubleClicked(heading);
            QCOMPARE(dialog.editSession()->draft().containers[0].contents.size(), before);
        }

        // A real row still adds.
        for (int i = 0; i < available->count(); ++i) {
            if (available->item(i)->data(Qt::UserRole).toString() == QLatin1String("meter.comp")) {
                available->setCurrentRow(i);
                break;
            }
        }
        add->click();
        const auto draft = dialog.editSession()->draft().containers[0].contents;
        QCOMPARE(draft.size(), before + 1);
        QCOMPARE(draft.last().typeId, QStringLiteral("meter.comp"));
        QCOMPARE(draft.last().name, QStringLiteral("Compression bar"));
        dialog.reject();
    }
};

QTEST_MAIN(TstContainerPickerCatalog)
#include "tst_container_picker_catalog.moc"
