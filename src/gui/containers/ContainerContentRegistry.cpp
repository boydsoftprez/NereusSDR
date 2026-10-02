// no-port-check: NereusSDR-original content catalog and lossless meter adapter.
#include "ContainerContentRegistry.h"
#include "gui/meters/MeterItem.h"
#include "core/UnbuiltFeatureList.h"
#include <QUuid>
#include <memory>
#include <cmath>
#include "gui/meters/SpacerItem.h"
#include "gui/meters/FadeCoverItem.h"
#include "gui/meters/LEDItem.h"
#include "gui/meters/HistoryGraphItem.h"
#include "gui/meters/MagicEyeItem.h"
#include "gui/meters/NeedleScalePwrItem.h"
#include "gui/meters/SignalTextItem.h"
#include "gui/meters/DialItem.h"
#include "gui/meters/TextOverlayItem.h"
#include "gui/meters/WebImageItem.h"
#include "gui/meters/FilterDisplayItem.h"
#include "gui/meters/RotatorItem.h"
#include "gui/meters/BandButtonItem.h"
#include "gui/meters/ModeButtonItem.h"
#include "gui/meters/FilterButtonItem.h"
#include "gui/meters/AntennaButtonItem.h"
#include "gui/meters/TuneStepButtonItem.h"
#include "gui/meters/OtherButtonItem.h"
#include "gui/meters/VoiceRecordPlayItem.h"
#include "gui/meters/VfoDisplayItem.h"
#include "gui/meters/ClockItem.h"
#include "gui/meters/ClickBoxItem.h"
#include "gui/meters/DataOutItem.h"
namespace NereusSDR {
namespace {
std::unique_ptr<MeterItem> allocate(const QString& type) {
    if (type == QLatin1String("BAR")) { return std::make_unique<BarItem>(); }
    if (type == QLatin1String("SOLID")) { return std::make_unique<SolidColourItem>(); }
    if (type == QLatin1String("IMAGE")) { return std::make_unique<ImageItem>(); }
    if (type == QLatin1String("SCALE")) { return std::make_unique<ScaleItem>(); }
    if (type == QLatin1String("TEXT")) { return std::make_unique<TextItem>(); }
    if (type == QLatin1String("NEEDLE")) { return std::make_unique<NeedleItem>(); }
    if (type == QLatin1String("SPACER")) { return std::make_unique<SpacerItem>(); }
    if (type == QLatin1String("FADECOVER")) { return std::make_unique<FadeCoverItem>(); }
    if (type == QLatin1String("LED")) { return std::make_unique<LEDItem>(); }
    if (type == QLatin1String("HISTORY")) { return std::make_unique<HistoryGraphItem>(); }
    if (type == QLatin1String("MAGICEYE")) { return std::make_unique<MagicEyeItem>(); }
    if (type == QLatin1String("NEEDLESCALEPWR")) { return std::make_unique<NeedleScalePwrItem>(); }
    if (type == QLatin1String("SIGNALTEXT")) { return std::make_unique<SignalTextItem>(); }
    if (type == QLatin1String("DIAL")) { return std::make_unique<DialItem>(); }
    if (type == QLatin1String("TEXTOVERLAY")) { return std::make_unique<TextOverlayItem>(); }
    if (type == QLatin1String("WEBIMAGE")) { return std::make_unique<WebImageItem>(); }
    if (type == QLatin1String("FILTERDISPLAY")) { return std::make_unique<FilterDisplayItem>(); }
    if (type == QLatin1String("ROTATOR")) { return std::make_unique<RotatorItem>(); }
    if (type == QLatin1String("BANDBTNS")) { return std::make_unique<BandButtonItem>(); }
    if (type == QLatin1String("MODEBTNS")) { return std::make_unique<ModeButtonItem>(); }
    if (type == QLatin1String("FILTERBTNS")) { return std::make_unique<FilterButtonItem>(); }
    if (type == QLatin1String("ANTENNABTNS")) { return std::make_unique<AntennaButtonItem>(); }
    if (type == QLatin1String("TUNESTEPBTNS")) { return std::make_unique<TuneStepButtonItem>(); }
    if (type == QLatin1String("OTHERBTNS")) { return std::make_unique<OtherButtonItem>(); }
    if (type == QLatin1String("VOICERECPLAY")) { return std::make_unique<VoiceRecordPlayItem>(); }
    if (type == QLatin1String("VFO")) { return std::make_unique<VfoDisplayItem>(); }
    if (type == QLatin1String("CLOCK")) { return std::make_unique<ClockItem>(); }
    if (type == QLatin1String("CLICKBOX")) { return std::make_unique<ClickBoxItem>(); }
    if (type == QLatin1String("DATAOUT")) { return std::make_unique<DataOutItem>(); }
    return {};
}
bool validBaseFields(const QString& raw) {
    const QStringList fields = raw.split(QLatin1Char('|'));
    if (fields.size() < 7) { return false; }
    for (int i = 1; i <= 4; ++i) {
        bool ok = false; const float value = fields[i].toFloat(&ok);
        if (!ok || !std::isfinite(value)) { return false; }
    }
    for (int i : {5,6}) {
        bool ok = false; fields[i].toInt(&ok); if (!ok) { return false; }
    }
    return true;
}
const char* kEntryProperty = "containerContentEntry";
}
QVector<ContentDescriptor> ContainerContentRegistry::descriptors() const {
    QVector<ContentDescriptor> result;
    result.append({QStringLiteral("BAR"), QStringLiteral("Bar"), false, true, {}});
    result.append({QStringLiteral("SOLID"), QStringLiteral("SolidColour"), false, true, {}});
    result.append({QStringLiteral("IMAGE"), QStringLiteral("Image"), false, true, {}});
    result.append({QStringLiteral("SCALE"), QStringLiteral("Scale"), false, true, {}});
    result.append({QStringLiteral("TEXT"), QStringLiteral("Text"), false, true, {}});
    result.append({QStringLiteral("NEEDLE"), QStringLiteral("Needle"), false, true, {}});
    result.append({QStringLiteral("SPACER"), QStringLiteral("Spacer"), false, true, {}});
    result.append({QStringLiteral("FADECOVER"), QStringLiteral("FadeCover"), false, true, {}});
    result.append({QStringLiteral("LED"), QStringLiteral("LED"), false, true, {}});
    result.append({QStringLiteral("HISTORY"), QStringLiteral("HistoryGraph"), false, true, {}});
    result.append({QStringLiteral("MAGICEYE"), QStringLiteral("MagicEye"), false, true, {}});
    result.append({QStringLiteral("NEEDLESCALEPWR"), QStringLiteral("NeedleScalePwr"), false, true, {}});
    result.append({QStringLiteral("SIGNALTEXT"), QStringLiteral("SignalText"), false, true, {}});
    result.append({QStringLiteral("DIAL"), QStringLiteral("Dial"), false, true, {}});
    result.append({QStringLiteral("TEXTOVERLAY"), QStringLiteral("TextOverlay"), false, true, {}});
    result.append({QStringLiteral("WEBIMAGE"), QStringLiteral("WebImage"), false, true, {}});
    result.append({QStringLiteral("FILTERDISPLAY"), QStringLiteral("FilterDisplay"), false, true, {}});
    result.append({QStringLiteral("ROTATOR"), QStringLiteral("Rotator"), false, true, {}});
    result.append({QStringLiteral("BANDBTNS"), QStringLiteral("BandButton"), false, true, {}});
    result.append({QStringLiteral("MODEBTNS"), QStringLiteral("ModeButton"), false, true, {}});
    result.append({QStringLiteral("FILTERBTNS"), QStringLiteral("FilterButton"), false, true, {}});
    result.append({QStringLiteral("ANTENNABTNS"), QStringLiteral("AntennaButton"), false, true, {}});
    result.append({QStringLiteral("TUNESTEPBTNS"), QStringLiteral("TuneStepButton"), false, true, {}});
    result.append({QStringLiteral("OTHERBTNS"), QStringLiteral("OtherButton"), false, true, {}});
    result.append({QStringLiteral("VOICERECPLAY"), QStringLiteral("VoiceRecordPlay"), false, true, {}});
    result.append({QStringLiteral("VFO"), QStringLiteral("VfoDisplay"), false, true, {}});
    result.append({QStringLiteral("CLOCK"), QStringLiteral("Clock"), false, true, {}});
    result.append({QStringLiteral("CLICKBOX"), QStringLiteral("ClickBox"), false, true, {}});
    result.append({QStringLiteral("DATAOUT"), QStringLiteral("DataOut"), false, true, {}});
    result.append({QStringLiteral("applet:rx"), QStringLiteral("RX"), true, true, {}});
    result.append({QStringLiteral("applet:Display"), QStringLiteral("Display"), true, true, {}});
    result.append({QStringLiteral("applet:TX"), QStringLiteral("TX"), true, true, {}});
    result.append({QStringLiteral("applet:PHCW"), QStringLiteral("Phone / CW"), true, true, {}});
    result.append({QStringLiteral("applet:RADE"), QStringLiteral("RADE"), true, true, {}});
    result.append({QStringLiteral("applet:vax"), QStringLiteral("VAX"), true, true, {}});
    result.append({QStringLiteral("applet:pure_signal"), QStringLiteral("PureSignal"), true, true, {}});
    result.append({QStringLiteral("applet:mod_monitor"), QStringLiteral("Mod Monitor"), true, true, {}});
    result.append({QStringLiteral("applet:tci"), QStringLiteral("TCI"), true, true, {}});
    result.append({QStringLiteral("applet:tci_clients"), QStringLiteral("Client Chain"), true, true, {}});
    result.append({QStringLiteral("applet:amp"), QStringLiteral("Amplifier"), true, true, {}});
    result.append({QStringLiteral("applet:tuner"), QStringLiteral("Tuner"), true, true, {}});
    result.append({QStringLiteral("applet:RfKit"), QStringLiteral("RF Kit"), true, true, {}});
    result.append({QStringLiteral("applet:s_meter"), QStringLiteral("S-Meter"), true, true, {}});
    for (auto& descriptor : result) {
        std::optional<UnbuiltFeature> feature;
        if (descriptor.typeId == QLatin1String("FILTERDISPLAY")) { feature = UnbuiltFeature::ContainerFilterDisplay; }
        if (descriptor.typeId == QLatin1String("CLICKBOX")) { feature = UnbuiltFeature::ContainerClickBox; }
        if (descriptor.typeId == QLatin1String("VOICERECPLAY")) { feature = UnbuiltFeature::Voice; }
        if (feature && !UnbuiltFeatures::isBuilt(*feature)) {
            descriptor.available = false; descriptor.unavailableReason = QStringLiteral("This capability is not built yet");
        }
    }
    result.append({QStringLiteral("DISCORDBTNS"), QStringLiteral("Discord control (removed)"), false, false, QStringLiteral("This control was retired")});
    return result;
}
ContentEntry ContainerContentRegistry::makeEntry(const QString& typeId) const {
    ContentEntry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.typeId = typeId;
    entry.name = typeId + QStringLiteral(" (unavailable)");
    for (const auto& descriptor : descriptors()) {
        if (descriptor.typeId == typeId) { entry.name = descriptor.title; break; }
    }
    const auto item = allocate(typeId);
    if (item) { entry.config.insert(QStringLiteral("legacyRecord"), item->serialize()); entry.canvasRect = QRectF(item->x(),item->y(),item->itemWidth(),item->itemHeight()); entry.paintOrder = item->zOrder(); }
    return entry;
}
QString ContainerContentRegistry::validateEntry(const ContentEntry& entry) const {
    // Unavailable records are valid recoverable data, never editing failures.
    if (entry.extensions.contains(QStringLiteral("unavailableReason"))) { return {}; }
    for (const auto& descriptor : descriptors()) { if (descriptor.typeId == entry.typeId && !descriptor.available) { return {}; } }
    if (!allocate(entry.typeId)) { return {}; }
    std::unique_ptr<MeterItem> item(createMeterItem(entry, nullptr, ContentRenderMode::Validation));
    return item ? QString() : QStringLiteral("Invalid legacy %1 record").arg(entry.typeId);
}
MeterItem* ContainerContentRegistry::createMeterItem(const ContentEntry& entry, QObject* parent,
                                                   ContentRenderMode mode) const {
    // Re-evaluate current support; a stored explanation is not a permanent veto.
    for (const auto& descriptor : descriptors()) {
        if (descriptor.typeId == entry.typeId && !descriptor.available) { return nullptr; }
    }
    auto item = allocate(entry.typeId);
    if (!item) { return nullptr; }
    auto* web = qobject_cast<WebImageItem*>(item.get());
    if (web) { web->setFetchEnabled(false); }
    const QString raw = entry.config.value(QStringLiteral("legacyRecord")).toString();
    if (!raw.isEmpty()) {
        if (!validBaseFields(raw) || raw.section(QLatin1Char('|'), 0, 0) != entry.typeId || !item->deserialize(raw)) { return nullptr; }
    }
    const QJsonObject overrides = entry.config.value(QStringLiteral("overrides")).toObject();
    if (!overrides.isEmpty()) {
        QStringList fields = item->serialize().split(QLatin1Char('|'));
        for (auto it = overrides.begin(); it != overrides.end(); ++it) {
            bool ok = false; const int index = it.key().toInt(&ok);
            if (!ok || index <= 0 || index >= fields.size() || !it.value().isString()) { return nullptr; }
            fields[index] = it.value().toString();
        }
        const QString effective = fields.join(QLatin1Char('|'));
        if (!validBaseFields(effective) || !item->deserialize(effective)) { return nullptr; }
    }
    if (entry.context.contains(QStringLiteral("bindingId"))) { item->setBindingId(entry.context.value(QStringLiteral("bindingId")).toInt()); }
    item->setMmioBinding(QUuid(entry.context.value(QStringLiteral("mmioGuid")).toString()), entry.context.value(QStringLiteral("mmioVariable")).toString());
    if (entry.context.contains(QStringLiteral("stackSlot"))) {
        item->setStackSlot(entry.context.value(QStringLiteral("stackSlot")).toInt(-1));
        item->setSlotLocalY(entry.context.value(QStringLiteral("slotLocalY")).toDouble());
        item->setSlotLocalH(entry.context.value(QStringLiteral("slotLocalH")).toDouble(1));
    }
    if (entry.context.contains(QStringLiteral("onlyWhenRx"))) { item->setOnlyWhenRx(entry.context.value(QStringLiteral("onlyWhenRx")).toBool()); }
    if (entry.context.contains(QStringLiteral("onlyWhenTx"))) { item->setOnlyWhenTx(entry.context.value(QStringLiteral("onlyWhenTx")).toBool()); }
    if (entry.context.contains(QStringLiteral("displayGroup"))) { item->setDisplayGroup(entry.context.value(QStringLiteral("displayGroup")).toInt()); }
    // Raw legacy geometry is authoritative until capture supplies an explicit rect.
    if (!entry.canvasRect.isNull()) { item->setRect(entry.canvasRect.x(), entry.canvasRect.y(), entry.canvasRect.width(), entry.canvasRect.height()); }
    item->setZOrder(entry.paintOrder);
    ContentEntry remembered = entry;
    if (remembered.id.isEmpty()) { remembered.id = QUuid::createUuid().toString(QUuid::WithoutBraces); }
    if (remembered.name.isEmpty()) { remembered.name = makeEntry(entry.typeId).name; }
    item->setProperty(kEntryProperty, QVariant::fromValue(remembered));
    if (web && mode == ContentRenderMode::Live) { web->setFetchEnabled(true); }
    if (mode != ContentRenderMode::Live) { item->blockSignals(true); }
    item->setParent(parent);
    return item.release();
}
ContentEntry ContainerContentRegistry::captureMeterItem(const MeterItem& item, const ContentEntry& prior) const {
    ContentEntry entry = prior.id.isEmpty() ? item.property(kEntryProperty).value<ContentEntry>() : prior;
    const QString serialized = item.serialize();
    if (entry.typeId.isEmpty()) { entry = makeEntry(serialized.section(QLatin1Char('|'),0,0)); entry.config.insert(QStringLiteral("legacyRecord"), serialized); }
    if (entry.id.isEmpty()) { entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces); }
    auto baseline = allocate(entry.typeId);
    if (auto* web = qobject_cast<WebImageItem*>(baseline.get())) { web->setFetchEnabled(false); }
    if (baseline && baseline->deserialize(entry.config.value(QStringLiteral("legacyRecord")).toString())) {
        const QStringList before = baseline->serialize().split(QLatin1Char('|'));
        const QStringList after = serialized.split(QLatin1Char('|'));
        QJsonObject overrides = entry.config.value(QStringLiteral("overrides")).toObject();
        for (int i = 1; i < after.size(); ++i) {
            if (i >= before.size() || after[i] != before[i]) { overrides.insert(QString::number(i), after[i]); }
            else { overrides.remove(QString::number(i)); }
        }
        entry.config.insert(QStringLiteral("overrides"), overrides);
    }
    entry.canvasRect = QRectF(item.x(),item.y(),item.itemWidth(),item.itemHeight());
    entry.paintOrder = item.zOrder();
    entry.context.insert(QStringLiteral("bindingId"),item.bindingId());
    entry.context.insert(QStringLiteral("mmioGuid"),item.mmioGuid().toString(QUuid::WithoutBraces));
    entry.context.insert(QStringLiteral("mmioVariable"),item.mmioVariable());
    entry.context.insert(QStringLiteral("stackSlot"),item.stackSlot());
    entry.context.insert(QStringLiteral("slotLocalY"),item.slotLocalY());
    entry.context.insert(QStringLiteral("slotLocalH"),item.slotLocalH());
    entry.context.insert(QStringLiteral("onlyWhenRx"),item.onlyWhenRx());
    entry.context.insert(QStringLiteral("onlyWhenTx"),item.onlyWhenTx());
    entry.context.insert(QStringLiteral("displayGroup"),item.displayGroup());
    return entry;
}
} // namespace NereusSDR
