// no-port-check: NereusSDR-original Thetis .dat container mapping.
#include "ThetisContainerFile.h"
#include "ContainerContentRegistry.h"
#include "ContainerDocumentCodec.h"
#include "LegacyContainerImporter.h"
#include "core/LogCategories.h"
#include <QCoreApplication>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>
#include "core/mmio/ExternalVariableEngine.h"
#include "core/mmio/MmioEndpoint.h"
#include <QUuid>
#include <algorithm>
#include <limits>

namespace NereusSDR {
namespace {

QString newId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

QString axisName(AxisLock axis)
{
    switch (axis) {
    case AxisLock::TopLeft: return QStringLiteral("TOPLEFT");
    case AxisLock::Top: return QStringLiteral("TOP");
    case AxisLock::TopRight: return QStringLiteral("TOPRIGHT");
    case AxisLock::Right: return QStringLiteral("RIGHT");
    case AxisLock::BottomRight: return QStringLiteral("BOTTOMRIGHT");
    case AxisLock::Bottom: return QStringLiteral("BOTTOM");
    case AxisLock::BottomLeft: return QStringLiteral("BOTTOMLEFT");
    case AxisLock::Left: break;
    }
    return QStringLiteral("LEFT");
}

AxisLock axisFromName(const QString& name)
{
    const QString value = name.trimmed().toUpper();
    if (value == QStringLiteral("TOPLEFT")) { return AxisLock::TopLeft; }
    if (value == QStringLiteral("TOP")) { return AxisLock::Top; }
    if (value == QStringLiteral("TOPRIGHT")) { return AxisLock::TopRight; }
    if (value == QStringLiteral("RIGHT")) { return AxisLock::Right; }
    if (value == QStringLiteral("BOTTOMRIGHT")) { return AxisLock::BottomRight; }
    if (value == QStringLiteral("BOTTOM")) { return AxisLock::Bottom; }
    if (value == QStringLiteral("BOTTOMLEFT")) { return AxisLock::BottomLeft; }
    return AxisLock::Left;
}

// Thetis MeterType <-> NereusSDR container type. Placement follows the item
// the operator sees; a reading or a face that NereusSDR does not have falls
// back to the closest bar.
QString nereusTypeFor(const QString& meterType)
{
    static const QHash<QString, QString> map{
        {QStringLiteral("SIGNAL_STRENGTH"), QStringLiteral("BAR")},
        {QStringLiteral("AVG_SIGNAL_STRENGTH"), QStringLiteral("BAR")},
        {QStringLiteral("SIGNAL_TEXT"), QStringLiteral("SIGNALTEXT")},
        {QStringLiteral("SIGNAL_MAX_BIN"), QStringLiteral("SIGNALTEXT")},
        {QStringLiteral("ADC"), QStringLiteral("BAR")},
        {QStringLiteral("AGC"), QStringLiteral("BAR")},
        {QStringLiteral("AGC_GAIN"), QStringLiteral("BAR")},
        {QStringLiteral("ESTIMATED_PBSNR"), QStringLiteral("BAR")},
        {QStringLiteral("MIC"), QStringLiteral("meter.mic")},
        {QStringLiteral("EQ"), QStringLiteral("BAR")},
        {QStringLiteral("LEVELER"), QStringLiteral("BAR")},
        {QStringLiteral("LEVELER_GAIN"), QStringLiteral("BAR")},
        {QStringLiteral("ALC"), QStringLiteral("meter.alc")},
        {QStringLiteral("ALC_GAIN"), QStringLiteral("BAR")},
        {QStringLiteral("ALC_GROUP"), QStringLiteral("BAR")},
        {QStringLiteral("CFC"), QStringLiteral("BAR")},
        {QStringLiteral("CFC_GAIN"), QStringLiteral("BAR")},
        {QStringLiteral("COMP"), QStringLiteral("BAR")},
        {QStringLiteral("PWR"), QStringLiteral("meter.powerSwr")},
        {QStringLiteral("REVERSE_PWR"), QStringLiteral("meter.powerSwr")},
        {QStringLiteral("SWR"), QStringLiteral("meter.powerSwr")},
        {QStringLiteral("MAGIC_EYE"), QStringLiteral("MAGICEYE")},
        {QStringLiteral("ANANMM"), QStringLiteral("meter.ananMulti")},
        {QStringLiteral("CROSS"), QStringLiteral("meter.crossNeedle")},
        {QStringLiteral("VFO_DISPLAY"), QStringLiteral("VFO")},
        {QStringLiteral("CLOCK"), QStringLiteral("CLOCK")},
        {QStringLiteral("SPACER"), QStringLiteral("SPACER")},
        {QStringLiteral("TEXT_OVERLAY"), QStringLiteral("TEXTOVERLAY")},
        {QStringLiteral("DATA_OUT"), QStringLiteral("DATAOUT")},
        {QStringLiteral("ROTATOR"), QStringLiteral("ROTATOR")},
        {QStringLiteral("LED"), QStringLiteral("LED")},
        {QStringLiteral("WEB_IMAGE"), QStringLiteral("WEBIMAGE")},
        {QStringLiteral("BAND_BUTTONS"), QStringLiteral("BANDBTNS")},
        {QStringLiteral("MODE_BUTTONS"), QStringLiteral("MODEBTNS")},
        {QStringLiteral("FILTER_BUTTONS"), QStringLiteral("FILTERBTNS")},
        {QStringLiteral("ANTENNA_BUTTONS"), QStringLiteral("ANTENNABTNS")},
        {QStringLiteral("HISTORY"), QStringLiteral("HISTORY")},
        {QStringLiteral("TUNESTEP_BUTTONS"), QStringLiteral("TUNESTEPBTNS")},
        {QStringLiteral("DISCORD_BUTTONS"), QStringLiteral("OTHERBTNS")},
        {QStringLiteral("FILTER_DISPLAY"), QStringLiteral("FILTERDISPLAY")},
        {QStringLiteral("DIAL_DISPLAY"), QStringLiteral("DIAL")},
        {QStringLiteral("CUSTOM_METER_BAR"), QStringLiteral("meter.customBar")},
        {QStringLiteral("OTHER_BUTTONS"), QStringLiteral("OTHERBTNS")},
        {QStringLiteral("WAVE_RECORD"), QStringLiteral("OTHERBTNS")},
        {QStringLiteral("VOICE_RECORD_PLAY_BUTTONS"), QStringLiteral("VOICERECPLAY")},
        {QStringLiteral("ACG_MAX_MAG"), QStringLiteral("BAR")},
    };
    return map.value(meterType, QStringLiteral("TEXTOVERLAY"));
}

QString thetisMeterTypeFor(const QString& typeId)
{
    static const QHash<QString, QString> map{
        {QStringLiteral("SIGNALTEXT"), QStringLiteral("SIGNAL_TEXT")},
        {QStringLiteral("meter.mic"), QStringLiteral("MIC")},
        {QStringLiteral("meter.alc"), QStringLiteral("ALC")},
        {QStringLiteral("meter.powerSwr"), QStringLiteral("PWR")},
        {QStringLiteral("MAGICEYE"), QStringLiteral("MAGIC_EYE")},
        {QStringLiteral("meter.ananMulti"), QStringLiteral("ANANMM")},
        {QStringLiteral("meter.crossNeedle"), QStringLiteral("CROSS")},
        {QStringLiteral("VFO"), QStringLiteral("VFO_DISPLAY")},
        {QStringLiteral("CLOCK"), QStringLiteral("CLOCK")},
        {QStringLiteral("SPACER"), QStringLiteral("SPACER")},
        {QStringLiteral("TEXTOVERLAY"), QStringLiteral("TEXT_OVERLAY")},
        {QStringLiteral("DATAOUT"), QStringLiteral("DATA_OUT")},
        {QStringLiteral("ROTATOR"), QStringLiteral("ROTATOR")},
        {QStringLiteral("LED"), QStringLiteral("LED")},
        {QStringLiteral("WEBIMAGE"), QStringLiteral("WEB_IMAGE")},
        {QStringLiteral("BANDBTNS"), QStringLiteral("BAND_BUTTONS")},
        {QStringLiteral("MODEBTNS"), QStringLiteral("MODE_BUTTONS")},
        {QStringLiteral("FILTERBTNS"), QStringLiteral("FILTER_BUTTONS")},
        {QStringLiteral("ANTENNABTNS"), QStringLiteral("ANTENNA_BUTTONS")},
        {QStringLiteral("HISTORY"), QStringLiteral("HISTORY")},
        {QStringLiteral("TUNESTEPBTNS"), QStringLiteral("TUNESTEP_BUTTONS")},
        {QStringLiteral("FILTERDISPLAY"), QStringLiteral("FILTER_DISPLAY")},
        {QStringLiteral("DIAL"), QStringLiteral("DIAL_DISPLAY")},
        {QStringLiteral("OTHERBTNS"), QStringLiteral("OTHER_BUTTONS")},
        {QStringLiteral("VOICERECPLAY"), QStringLiteral("VOICE_RECORD_PLAY_BUTTONS")},
        {QStringLiteral("BAR"), QStringLiteral("CUSTOM_METER_BAR")},
        {QStringLiteral("meter.customBar"), QStringLiteral("CUSTOM_METER_BAR")},
    };
    return map.value(typeId, QStringLiteral("TEXT_OVERLAY"));
}

QJsonArray rectArray(const QRectF& rect)
{
    return QJsonArray{rect.x(), rect.y(), rect.width(), rect.height()};
}

QRectF rectFrom(const QJsonValue& value, const QRectF& fallback = QRectF())
{
    const QJsonArray array = value.toArray();
    if (array.size() != 4) { return fallback; }
    return QRectF(array[0].toDouble(), array[1].toDouble(), array[2].toDouble(), array[3].toDouble());
}

QJsonObject entryJson(const ContentEntry& entry)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), entry.id);
    object.insert(QStringLiteral("typeId"), entry.typeId);
    object.insert(QStringLiteral("name"), entry.name);
    object.insert(QStringLiteral("context"), entry.context);
    object.insert(QStringLiteral("config"), entry.config);
    object.insert(QStringLiteral("extensions"), entry.extensions);
    object.insert(QStringLiteral("canvasRect"), rectArray(entry.canvasRect));
    object.insert(QStringLiteral("paintOrder"), entry.paintOrder);
    object.insert(QStringLiteral("visible"), entry.visible);
    if (entry.returnLocation) {
        object.insert(QStringLiteral("returnLocation"), QJsonObject{
            {QStringLiteral("containerId"), entry.returnLocation->containerId},
            {QStringLiteral("beforeId"), entry.returnLocation->beforeId},
            {QStringLiteral("afterId"), entry.returnLocation->afterId},
            {QStringLiteral("extensions"), entry.returnLocation->extensions}});
    }
    return object;
}

ContentEntry entryFrom(const QJsonObject& object)
{
    ContentEntry entry;
    entry.id = object.value(QStringLiteral("id")).toString();
    entry.typeId = object.value(QStringLiteral("typeId")).toString();
    entry.name = object.value(QStringLiteral("name")).toString();
    entry.context = object.value(QStringLiteral("context")).toObject();
    entry.config = object.value(QStringLiteral("config")).toObject();
    entry.extensions = object.value(QStringLiteral("extensions")).toObject();
    entry.canvasRect = rectFrom(object.value(QStringLiteral("canvasRect")));
    entry.paintOrder = object.value(QStringLiteral("paintOrder")).toInt();
    entry.visible = object.value(QStringLiteral("visible")).toBool(true);
    if (object.value(QStringLiteral("returnLocation")).isObject()) {
        const QJsonObject location = object.value(QStringLiteral("returnLocation")).toObject();
        ReturnLocation returned;
        returned.containerId = location.value(QStringLiteral("containerId")).toString();
        returned.beforeId = location.value(QStringLiteral("beforeId")).toString();
        returned.afterId = location.value(QStringLiteral("afterId")).toString();
        returned.extensions = location.value(QStringLiteral("extensions")).toObject();
        entry.returnLocation = returned;
    }
    if (entry.id.isEmpty()) { entry.id = newId(); }
    if (entry.typeId.isEmpty()) { entry.typeId = QStringLiteral("TEXTOVERLAY"); }
    return entry;
}

bool runBridge(const QString& mode, const QByteArray& input, QByteArray* output, QString* error)
{
    const QString bridge = ThetisContainerFile::bridgePath();
    if (bridge.isEmpty()) {
        if (error) { *error = QStringLiteral("Thetis .dat bridge is not installed."); }
        return false;
    }
    QTemporaryDir temporary;
    if (!temporary.isValid()) {
        if (error) { *error = QStringLiteral("Could not create a temporary folder."); }
        return false;
    }
    const QString inputPath = temporary.filePath(QStringLiteral("input"));
    const QString outputPath = temporary.filePath(QStringLiteral("output"));
    {
        QFile file(inputPath);
        if (!file.open(QIODevice::WriteOnly)) {
            if (error) { *error = QStringLiteral("Could not write the bridge input."); }
            return false;
        }
        file.write(input);
    }
    QProcess process;
    process.start(bridge, {mode, inputPath, outputPath});
    if (!process.waitForStarted(5000) || !process.waitForFinished(20000)) {
        process.kill();
        process.waitForFinished(2000);
        if (error) { *error = QStringLiteral("The Thetis .dat bridge did not finish."); }
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            *error = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            if (error->isEmpty()) { *error = QStringLiteral("The Thetis .dat bridge failed."); }
        }
        return false;
    }
    QFile file(outputPath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) { *error = QStringLiteral("Could not read the bridge output."); }
        return false;
    }
    if (output) { *output = file.readAll(); }
    return true;
}

// ---- clsIGSettings dictionary -> NereusSDR item record -------------------
// Thetis stores each item's own settings in ItemGroupSettings_2_ as a .NET
// ConcurrentDictionary<string,object>; the bridge turns it into JSON.  The
// meter items were ported from Thetis with the same field meanings, so the
// decoded dictionary is written into the legacy record the item deserializes.

bool readThetisSettings(const QString& text, QJsonObject* settings)
{
    if (text.isEmpty() || !settings) { return false; }
    QByteArray json;
    QString error;
    // The bridge writes a UTF-8 JSON file and its "settings" mode takes the
    // raw "1|..." value as input.
    const QByteArray input = text.toUtf8();
    if (!runBridge(QStringLiteral("settings"), input, &json, &error)) {
        qCWarning(lcContainer) << "Thetis item settings:" << error;
        return false;
    }
    *settings = QJsonDocument::fromJson(json).object();
    return true;
}

QString settingString(const QJsonObject& settings, const char* key, const QString& fallback = {})
{
    const QJsonValue value = settings.value(QLatin1String(key));
    return value.isString() ? value.toString() : fallback;
}

bool settingBool(const QJsonObject& settings, const char* key, bool fallback = false)
{
    const QJsonValue value = settings.value(QLatin1String(key));
    return value.isBool() ? value.toBool() : fallback;
}

int settingInt(const QJsonObject& settings, const char* key, int fallback = 0)
{
    const QJsonValue value = settings.value(QLatin1String(key));
    return value.isDouble() ? value.toInt() : fallback;
}

double settingDouble(const QJsonObject& settings, const char* key, double fallback = 0.0)
{
    const QJsonValue value = settings.value(QLatin1String(key));
    return value.isDouble() ? value.toDouble() : fallback;
}

// FontStyle is emitted as {"$enum":"FontStyle","name":"Bold","value":1};
// accept the plain number too.
int fontStyleFlags(const QJsonObject& settings, const char* key)
{
    const QJsonValue value = settings.value(QLatin1String(key));
    if (value.isDouble()) { return value.toInt(); }
    if (value.isObject()) { return value.toObject().value(QStringLiteral("value")).toInt(); }
    return 0;
}

QString colourSetting(const QJsonObject& settings, const char* key, const QString& fallback)
{
    const QString value = settingString(settings, key, fallback);
    return QColor(value).isValid() ? value : fallback;
}

// Thetis serializes a container colour as "A.R.G.B" (Common.ColourToString).
QString thetisColourToHex(const QString& text)
{
    const QStringList parts = text.split(QLatin1Char('.'));
    if (parts.size() == 4) {
        bool ok = false;
        const int a = parts[0].toInt(&ok);
        const int r = ok ? parts[1].toInt(&ok) : 0;
        const int g = ok ? parts[2].toInt(&ok) : 0;
        const int b = ok ? parts[3].toInt(&ok) : 0;
        if (ok) {
            return QColor(r, g, b, a).name(QColor::HexArgb);
        }
    }
    const QColor colour(text);
    return colour.isValid() ? colour.name(QColor::HexArgb) : QString();
}

QString hexToThetisColour(const QString& text)
{
    const QColor colour(text);
    if (!colour.isValid()) {
        return text.split(QLatin1Char('.')).size() == 4
            ? text : QStringLiteral("0.255.255.255");
    }
    return QStringLiteral("%1.%2.%3.%4")
        .arg(colour.alpha()).arg(colour.red()).arg(colour.green()).arg(colour.blue());
}

QString base64Text(const QString& text)
{
    return QString::fromLatin1(text.toUtf8().toBase64());
}

// TEXTOVERLAY field order: see TextOverlayItem::serialize().
QString textOverlayRecord(const ContentEntry& entry, const QJsonObject& settings,
                          int windowWidth, int windowHeight, bool noPanel = false,
                          double maxFontOverride = 0.0)
{
    const QString text1 = settingString(settings, "_text_1");
    const QString text2 = settingString(settings, "_text_2", text1);
    const QString colour = colourSetting(settings, "_colour", QStringLiteral("#FFC0C0C0"));
    const QString titleColour = colourSetting(settings, "_titleColor", QStringLiteral("#FF202020"));
    const QString historyColour = colourSetting(settings, "_historyColor", titleColour);
    const QString textBack1 = colourSetting(settings, "_subMarkerColour", QStringLiteral("#00000000"));
    const QString textBack2 = colourSetting(settings, "_peakValueColour", textBack1);
    const QString family1 = settingString(settings, "_font_family_1", QStringLiteral("Trebuchet MS"));
    const QString family2 = settingString(settings, "_font_family_2", family1);
    const int style1 = fontStyleFlags(settings, "_font_style_1");
    const int style2 = fontStyleFlags(settings, "_font_style_2");
    // TextOverlayItem scales its saved font with the meter's width. A Thetis
    // font chosen for the large native window then overflows its shorter
    // imported row, so keep each line inside its item height. The ratio is
    // scale-invariant, so it holds after the helper window is fitted.
    double maxFont = 18.0;
    if (windowWidth > 0 && windowHeight > 0 && entry.canvasRect.height() > 0.0) {
        const double aspect = double(windowHeight) / double(windowWidth);
        maxFont = entry.canvasRect.height() * aspect * 0.78 * 832.0 / 2.1;
        maxFont = qMax(6.0, maxFont);
    }
    if (maxFontOverride > 0.0) { maxFont = maxFontOverride; }
    const double size1 = qBound(1.0, settingDouble(settings, "_font_size_1", 18.0), maxFont);
    const double size2 = qBound(1.0, settingDouble(settings, "_font_size_2", size1), maxFont);
    QStringList p;
    p << QStringLiteral("TEXTOVERLAY")
      << QString::number(entry.canvasRect.x()) << QString::number(entry.canvasRect.y())
      << QString::number(entry.canvasRect.width()) << QString::number(entry.canvasRect.height())
      << QStringLiteral("0") << QString::number(entry.paintOrder)
      << colour << colour << titleColour << historyColour
      << QString::number(!noPanel && settingBool(settings, "_showSubMarker", true) ? 1 : 0)
      << family1 << QString::number(size1) << QString::number(style1 & 1)
      << family2 << QString::number(size2) << QString::number(style2 & 1)
      << QStringLiteral("0") << QStringLiteral("0.01")
      << base64Text(text1) << base64Text(text2)
      << textBack1 << textBack2
      << QString::number(settingBool(settings, "_showMarker", false) ? 1 : 0)
      << QString::number(settingBool(settings, "_showType", false) ? 1 : 0)
      // Thetis text overlays show RX text (text1) or TX text (text2), never
      // both halves at once.
      << QStringLiteral("1") << QStringLiteral("0") << QStringLiteral("0") << QStringLiteral("0") << QStringLiteral("0")
      << QString::number((style1 & 2) ? 1 : 0) << QString::number((style1 & 4) ? 1 : 0) << QString::number((style1 & 8) ? 1 : 0)
      << QString::number((style2 & 2) ? 1 : 0) << QString::number((style2 & 4) ? 1 : 0) << QString::number((style2 & 8) ? 1 : 0)
      << QString::number(settingBool(settings, "textoverlay_rx_ledlogic") ? 1 : 0)
      << QString::number(settingBool(settings, "textoverlay_tx_ledlogic") ? 1 : 0)
      << settingString(settings, "textoverlay_rx_4char")
      << settingString(settings, "textoverlay_tx_4char")
      // 2026-11-12: the WaveNode centre column is a stack of centred rows.
      << QString::number(settingBool(settings, "_centre", false) ? 1 : 0);
    return p.join(QLatin1Char('|'));
}

// LED field order: see LEDItem::serialize().
// Thetis draws a LED from its group width, not its group height: the eye
// is placed by _eyeScale and sized by _attack / _decay
// (MeterManager.cs:27442-27446). The saved _eyeBezelScale is a negative
// vertical offset in some layouts, which puts the eye above the item; keep
// the LED in its group slot instead, centred vertically, so it stays on
// screen and below the text overlays.
void applyLedSettings(ContentEntry& entry, const QJsonObject& settings,
                      int windowWidth, int windowHeight)
{
    if (windowWidth <= 0 || windowHeight <= 0) { return; }
    const double offsetX = qBound(0.0, settingDouble(settings, "_eyeScale", 0.5), 1.0);
    double sizeX = qAbs(settingDouble(settings, "_attack", 0.05));
    double sizeY = qAbs(settingDouble(settings, "_decay", 0.05));
    if (sizeX <= 0.0) { sizeX = 0.05; }
    if (sizeY <= 0.0) { sizeY = 0.05; }
    const double padding = qBound(0.001, settingDouble(settings, "_spacerPadding", 0.1), 10.0);

    // HF-Auto's two LEDs are the C/L selection windows in the
    // "C=... L=..." line. Their saved group sits below the text, but the
    // indicator belongs over the C or L value. Place a window there.
    const QString condition = settingString(settings, "_text_1");
    const int selection = condition.indexOf(QStringLiteral("StepperMotorSelection%"));
    if (selection >= 0) {
        const QString tail = condition.mid(selection);
        const bool cap = tail.contains(QStringLiteral("== 1"));
        const bool ind = tail.contains(QStringLiteral("== 2"));
        if (cap || ind) {
            const double containerWidth = qMax(1.0, double(windowWidth));
            const double containerHeight = qMax(1.0, double(windowHeight));
            // Keep the flash around the value, not past it: the saved
            // attack/decay sizes are wider than a "C=119"/"L=333" field.
            const double windowWidthPx = qMax(24.0,
                qMin(sizeX * containerWidth, 0.16 * containerWidth));
            const double windowHeightPx = qMax(1.0, sizeY * containerWidth);
            // The C window grows to the left and the L window grows to the
            // right, so each stays around its value without crossing the
            // other field. The row is y = 0.2125..0.3160.
            const double centreX = (cap ? 0.075 : 0.27) * containerWidth;
            const double centreY = 0.264 * containerHeight;
            entry.canvasRect = QRectF((centreX - windowWidthPx / 2.0) / containerWidth,
                                      (centreY - windowHeightPx / 2.0) / containerHeight,
                                      windowWidthPx / containerWidth,
                                      windowHeightPx / containerHeight);
            entry.config.insert(QStringLiteral("ledOffsetX"), 0.5);
            entry.config.insert(QStringLiteral("ledOffsetY"),
                                windowHeightPx / (2.0 * windowWidthPx));
            entry.config.insert(QStringLiteral("ledSizeX"), 1.0);
            entry.config.insert(QStringLiteral("ledSizeY"),
                                windowHeightPx / windowWidthPx);
            entry.config.insert(QStringLiteral("ledPadding"), padding);
            // Only the selected C or L window is drawn; the other one must
            // not cover its value with the off colour.
            entry.config.insert(QStringLiteral("ledShowFalse"), false);
            return;
        }
    }

    const double widthPx = qMax(1.0, entry.canvasRect.width() * windowWidth);
    // The group's own height is zero. Give it a band tall enough for the
    // panel (width times padding) and for the light itself.
    const double bandPx = qMax(padding * widthPx, sizeY * widthPx);
    const double heightPx = qMax(entry.canvasRect.height() * windowHeight, bandPx);
    entry.canvasRect.setHeight(heightPx / windowHeight);

    entry.config.insert(QStringLiteral("ledOffsetX"), offsetX);
    entry.config.insert(QStringLiteral("ledOffsetY"), (heightPx / 2.0) / widthPx);
    entry.config.insert(QStringLiteral("ledSizeX"), sizeX);
    entry.config.insert(QStringLiteral("ledSizeY"), sizeY);
    entry.config.insert(QStringLiteral("ledPadding"), padding);
}

QString ledRecord(const ContentEntry& entry, const QJsonObject& settings)
{
    const QString condition = settingString(settings, "_text_1");
    const QString trueColour = colourSetting(settings, "_colour", QStringLiteral("#FFC0C0C0"));
    const QString falseColour = colourSetting(settings, "_markerColour",
                                              colourSetting(settings, "_falseColour",
                                                            QStringLiteral("#FF202020")));
    const QString panelBack1 = colourSetting(settings, "_titleColor", QStringLiteral("#FF202020"));
    const QString panelBack2 = colourSetting(settings, "_historyColor", QStringLiteral("#FF202020"));
    const int historyMode = settingInt(settings, "_ignoreHistoryDuration", 0);
    const double offsetX = entry.config.value(QStringLiteral("ledOffsetX")).toDouble();
    const double offsetY = entry.config.value(QStringLiteral("ledOffsetY")).toDouble();
    const double sizeX = entry.config.value(QStringLiteral("ledSizeX")).toDouble();
    const double sizeY = entry.config.value(QStringLiteral("ledSizeY")).toDouble();
    const double padding = entry.config.value(QStringLiteral("ledPadding")).toDouble();
    QStringList p;
    p << QStringLiteral("LED")
      << QString::number(entry.canvasRect.x()) << QString::number(entry.canvasRect.y())
      << QString::number(entry.canvasRect.width()) << QString::number(entry.canvasRect.height())
      << QStringLiteral("0") << QString::number(entry.paintOrder)
      << QStringLiteral("0") << QStringLiteral("0")
      << trueColour << falseColour
      << panelBack1 << panelBack2
      << QString::number(settingBool(settings, "_showSubMarker", true) ? 1 : 0)
      << QString::number(historyMode == 1 ? 1 : 0)
      << QString::number(historyMode == 2 ? 1 : 0)
      << QString::number(padding, 'f', 4)
      << QStringLiteral("0") << QStringLiteral("0") << QStringLiteral("0")
      << QStringLiteral("1")
      << QString::number(offsetX, 'f', 6) << QString::number(offsetY, 'f', 6)
      << QString::number(sizeX, 'f', 6) << QString::number(sizeY, 'f', 6)
      << QString::number(settingBool(settings, "_peakHold", true) ? 1 : 0)
      << QString::number(entry.config.contains(QStringLiteral("ledShowFalse"))
                             ? (entry.config.value(QStringLiteral("ledShowFalse")).toBool() ? 1 : 0)
                             : (settingBool(settings, "_showMarker", true) ? 1 : 0))
      << QString::number(settingBool(settings, "led_process_when_hidden") ? 1 : 0)
      << base64Text(condition)
      << QString::number(settingBool(settings, "led_notx_true") ? 1 : 0)
      << QString::number(settingBool(settings, "led_notx_false") ? 1 : 0)
      << QString();
    return p.join(QLatin1Char('|'));
}

// Binds one Thetis MMIO variable ("Zjjg.fwdpwr1") to the NereusSDR endpoint
// that supplied it. Index 0 is the item's primary binding; later indices go
// into context.mmioVariables for the extra inputs.
void bindMmio(ContentEntry& entry, const QString& variable, int index)
{
    const int dot = variable.indexOf(QLatin1Char('.'));
    if (dot <= 0) { return; }
    const QString fourChar = variable.left(dot);
    const QString name = variable.mid(dot + 1);
    if (name.isEmpty() || name == QLatin1String("--DEFAULT--")) { return; }
    MmioEndpoint* endpoint = ExternalVariableEngine::instance().endpointByFourChar(fourChar);
    if (!endpoint) { return; }
    const QString guid = endpoint->guid().toString(QUuid::WithoutBraces);
    if (index == 0) {
        entry.context.insert(QStringLiteral("mmioGuid"), guid);
        entry.context.insert(QStringLiteral("mmioVariable"), name);
    } else {
        QJsonArray variables = entry.context.value(QStringLiteral("mmioVariables")).toArray();
        while (variables.size() < index) { variables.append(QJsonObject{}); }
        variables[index - 1] = QJsonObject{{QStringLiteral("guid"), guid},
                                           {QStringLiteral("variable"), name}};
        entry.context.insert(QStringLiteral("mmioVariables"), variables);
    }
}

// Face::Cross is a CompositePresetItem with Forward/Reflected channels. Thetis
// stores the two MMIO variables and the needle colours in the same dictionary.
void applyCrossSettings(ContentEntry& entry, const QJsonObject& settings)
{
    // Start from the real new-item Cross configuration: it carries the
    // schema, face, channels and every key the validator requires. The
    // Thetis values are applied on top, so the item is never rejected.
    ContainerContentRegistry registry;
    ContentEntry defaults = registry.makeEntry(QStringLiteral("meter.crossNeedle"));
    QJsonObject properties = defaults.config.value(QStringLiteral("properties")).toObject();
    if (properties.isEmpty()) { return; }

    const QString title = settingString(settings, "_titleText", QStringLiteral("RF"));
    // Thetis's cross meters carry a DarkMode flag. A dark face is what the
    // user sees there; the text and scale use the warm ink Thetis shows on
    // that face instead of the dim grey.
    const bool darkMode = settingBool(settings, "_darkMode", false);
    // The Cross face treats title, units and the v/max range as fixed; only
    // its editable keys may be sent, or applyConfiguration() rejects the
    // whole item.
    properties.insert(QStringLiteral("titleColor"),
                      darkMode ? QStringLiteral("#FFFFD24A")
                               : colourSetting(settings, "_titleColor", QStringLiteral("#FFC0C0C0")));
    properties.insert(QStringLiteral("lowColor"),
                      darkMode ? QStringLiteral("#FFFFD24A")
                               : colourSetting(settings, "_lowColor", QStringLiteral("#FF203040")));
    properties.insert(QStringLiteral("highColor"),
                      colourSetting(settings, "_highColor", QStringLiteral("#FFCC4040")));
    properties.insert(QStringLiteral("backdropColor"),
                      darkMode ? QStringLiteral("#FF1B1B1B")
                               : colourSetting(settings, "_backdropColour", QStringLiteral("#FFF1EEE2")));
    properties.insert(QStringLiteral("darkMode"), darkMode);
    properties.insert(QStringLiteral("powerScaleColor"),
                      colourSetting(settings, "_powerScaleColour", QStringLiteral("#FF808080")));
    properties.insert(QStringLiteral("subNeedleColor"),
                      colourSetting(settings, "_subMarkerColour", QStringLiteral("#FF00FF00")));
    properties.insert(QStringLiteral("shadow"), settingBool(settings, "_shadow", true));
    properties.insert(QStringLiteral("showPeakValue"), settingBool(settings, "_peakValue", true));
    properties.insert(QStringLiteral("powerLimitW"),
                      qMax(0.0, settingDouble(settings, "_maxPower", 0.0)));

    QJsonArray channels = properties.value(QStringLiteral("channels")).toArray();
    if (!channels.isEmpty()) {
        QJsonObject forward = channels[0].toObject();
        forward.insert(QStringLiteral("name"), title);
        forward.insert(QStringLiteral("units"), QStringLiteral("W"));
        forward.insert(QStringLiteral("color"),
                       colourSetting(settings, "_markerColour", QStringLiteral("#FFFF0000")));
        forward.insert(QStringLiteral("historyMs"),
                       qMax(100, settingInt(settings, "_historyDuration", 4000)));
        forward.insert(QStringLiteral("historyColor"),
                       colourSetting(settings, "_historyColor", QStringLiteral("#60FF0000")));
        channels[0] = forward;
    }
    if (channels.size() > 1) {
        QJsonObject reflected = channels[1].toObject();
        reflected.insert(QStringLiteral("name"), QStringLiteral("Reflected"));
        reflected.insert(QStringLiteral("units"), QStringLiteral("W"));
        reflected.insert(QStringLiteral("color"),
                         colourSetting(settings, "_subMarkerColour", QStringLiteral("#FF00FF00")));
        reflected.insert(QStringLiteral("historyMs"),
                         qMax(100, settingInt(settings, "_historyDuration", 4000)));
        reflected.insert(QStringLiteral("historyColor"),
                         colourSetting(settings, "_historyColor", QStringLiteral("#60FF0000")));
        channels[1] = reflected;
    }
    properties.insert(QStringLiteral("channels"), channels);

    entry.name = title;
    entry.config.remove(QStringLiteral("legacyRecord"));
    entry.config.insert(QStringLiteral("properties"), properties);
    bindMmio(entry, settingString(settings, "_mmio_variable_0"), 0);
    bindMmio(entry, settingString(settings, "_mmio_variable_1"), 1);
}

// Thetis positions an item from ItemGroupData plus the per-item offset and
// width scale. A pixel-layout container (MeterData field 9) works in the
// item's PixelScale space; a normal container works in fractions of the
// window width. Both end up as the normalized rect LegacyCanvas draws.
void applyThetisGeometry(ContentEntry& entry, const QJsonObject& settings,
                         int windowWidth, int windowHeight,
                         bool pixelLayout, double pixelScale, double* pixelTop)
{
    if (windowWidth <= 0 || windowHeight <= 0) { return; }
    const double offsetX = settingDouble(settings, "item_offset_x", 0.0);
    const double offsetY = settingDouble(settings, "item_offset_y", 0.0);
    const double widthScale = settingDouble(settings, "item_width_scale", 1.0);
    const double itemScale = settingDouble(settings, "item_pixel_scale", 0.0);
    const double groupX = entry.canvasRect.x();
    const double groupY = entry.canvasRect.y();
    const double groupW = entry.canvasRect.width();
    const double groupH = entry.canvasRect.height();
    double x = groupX;
    double y = groupY;
    double w = groupW;
    double h = groupH;
    if (pixelLayout) {
        const double scale = itemScale > 0.0 ? itemScale
                                             : (pixelScale > 0.0 ? pixelScale : 360.0);
        // Thetis lays a pixel container out in one vertical run
        // (MeterManager.cs RebuildPixel): each item group starts at the
        // natural bottom of the groups before it and its saved offset is a
        // drag from there. Items with large negative offsets were pulled up
        // over that run, so the running top is required to reproduce the
        // saved layout; offsets alone place items far too high.
        const double top = pixelTop ? *pixelTop : 0.0;
        x = (groupX * scale + offsetX) / windowWidth;
        y = (groupY * scale + top + offsetY) / windowHeight;
        w = (groupW * widthScale * scale) / windowWidth;
        h = (groupH * scale) / windowHeight;
        // The next group starts after this item's natural (undraged) bottom.
        const double naturalBottom = groupY * scale + top + groupH * scale;
        if (pixelTop && naturalBottom > *pixelTop) { *pixelTop = naturalBottom; }
    } else {
        x += offsetX;
        y += offsetY;
        w *= widthScale;
    }
    entry.canvasRect = QRectF(x, y, w, h);
}

// CUSTOM_METER_BAR is a BarPresetItem. Its saved record is JSON; the Thetis
// dictionary carries the title, range, units and one MMIO variable.
void applyCustomBarSettings(ContentEntry& entry, const QJsonObject& settings,
                            bool showTitle = true)
{
    const QString label = settingString(settings, "meter_custom_title", entry.name);
    double minimum = settingDouble(settings, "meter_custom_min", 0.0);
    double maximum = settingDouble(settings, "meter_custom_max", 100.0);
    if (!(maximum > minimum)) { maximum = minimum + 1.0; }
    int styleValue = 0;
    const QJsonValue style = settings.value(QStringLiteral("_barStyle"));
    if (style.isObject()) { styleValue = style.toObject().value(QStringLiteral("value")).toInt(); }
    else if (style.isDouble()) { styleValue = style.toInt(); }
    const QString barStyle = styleValue == 4 ? QStringLiteral("Segments")
        : styleValue == 3 ? QStringLiteral("GradientFilled")
        : styleValue == 2 ? QStringLiteral("SolidFilled")
        : QStringLiteral("Line");
    QJsonObject record{
        {QStringLiteral("kind"), QStringLiteral("BarPreset")},
        {QStringLiteral("face"), QStringLiteral("Classic")},
        {QStringLiteral("flavor"), QStringLiteral("Custom")},
        {QStringLiteral("label"), label},
        {QStringLiteral("bindingId"), -1},
        {QStringLiteral("secondaryBindingId"), -1},
        {QStringLiteral("x"), entry.canvasRect.x()},
        {QStringLiteral("y"), entry.canvasRect.y()},
        // Normalized Canvas sizes are fractions, not pixels: clamping them
        // to 1.0 made every imported bar paint at full-container size.
        {QStringLiteral("w"), qMax(0.0001, entry.canvasRect.width())},
        {QStringLiteral("h"), qMax(0.0001, entry.canvasRect.height())},
        {QStringLiteral("minValue"), minimum},
        {QStringLiteral("maxValue"), maximum},
        {QStringLiteral("redThreshold"), maximum},
        {QStringLiteral("units"), settingString(settings, "meter_custom_units")},
        {QStringLiteral("style"), barStyle},
        {QStringLiteral("barColor"), colourSetting(settings, "_markerColour", QStringLiteral("#FFC0C0C0"))},
        {QStringLiteral("titleColor"), colourSetting(settings, "_titleColor", QStringLiteral("#FF202020"))},
        {QStringLiteral("backdropColor"), colourSetting(settings, "_colour", QStringLiteral("#FF202020"))},
        {QStringLiteral("historyColor"), colourSetting(settings, "_historyColor", QStringLiteral("#60FF0000"))},
        {QStringLiteral("lowColor"), colourSetting(settings, "_lowColor", QStringLiteral("#FFFFFFFF"))},
        {QStringLiteral("highColor"), colourSetting(settings, "_highColor", QStringLiteral("#FFFFFF00"))},
        {QStringLiteral("subColor"), colourSetting(settings, "_subMarkerColour", QStringLiteral("#FFA9A9A9"))},
        {QStringLiteral("showHistory"), settingBool(settings, "_showHistory", true)},
        {QStringLiteral("peakHold"), settingBool(settings, "_peakHold", false)},
        {QStringLiteral("showValue"), true},
        {QStringLiteral("showPeakValue"), settingBool(settings, "_peakValue", true)},
        {QStringLiteral("showMarker"), settingBool(settings, "_showMarker", true)},
        {QStringLiteral("showSub"), settingBool(settings, "_showSubMarker", true)},
        {QStringLiteral("showTitle"), showTitle},
        {QStringLiteral("updateIntervalMs"), qMax(20, settingInt(settings, "_updateInterval", 50))},
        {QStringLiteral("historyMs"), qMax(100, settingInt(settings, "_historyDuration", 4000))},
        {QStringLiteral("ignoreHistoryMs"), qMax(0, settingInt(settings, "_ignoreHistoryDuration", 2000))},
        {QStringLiteral("attack"), settingDouble(settings, "_attack", 0.2)},
        {QStringLiteral("decay"), settingDouble(settings, "_decay", 0.1)}
    };
    entry.name = label;
    entry.config.insert(QStringLiteral("legacyRecord"),
                        QString::fromUtf8(QJsonDocument(record).toJson(QJsonDocument::Compact)));
    bindMmio(entry, settingString(settings, "_mmio_variable_0"), 0);
}

// OTHER_BUTTONS is a ButtonBoxItem. Its saved record holds the twelve Thetis
// group bitfields, the button order and the macro definitions. Building the
// record keeps the grid's own buttons and captions instead of the default
// button set.
QString otherButtonsRecord(const ContentEntry& entry, const QJsonObject& settings)
{
    const int columns = qMax(1, settingInt(settings, "buttonbox_columns", 6));
    QString out = QStringLiteral("OTHERBTNS|%1|%2|%3|%4|0|%5|%6|0")
        .arg(entry.canvasRect.x(), 0, 'g', 12)
        .arg(entry.canvasRect.y(), 0, 'g', 12)
        .arg(entry.canvasRect.width(), 0, 'g', 12)
        .arg(entry.canvasRect.height(), 0, 'g', 12)
        .arg(entry.paintOrder)
        .arg(columns);
    // Fields 9-14: border, margin, corner radius, row height and fades.
    // Thetis's 0.05 border and margin are a fraction of the whole grid
    // width. On a four-button row that leaves cells too narrow for their
    // captions, so the imported grid uses a smaller frame.
    out += QStringLiteral("|%1|%2|%3|%4|%5|%6")
        .arg(qMin(0.005, settingDouble(settings, "buttonbox_border", 0.05)))
        .arg(qMin(0.005, settingDouble(settings, "buttonbox_margin", 0.05)))
        .arg(settingDouble(settings, "buttonbox_radius", 0.1))
        .arg(settingDouble(settings, "buttonbox_height_ratio", 0.5))
        .arg(settingBool(settings, "_fadeOnRx", false) ? 1 : 0)
        .arg(settingBool(settings, "_fadeOnTx", false) ? 1 : 0);
    // Field 15: Thetis button numbers, then the twelve group bitfields.
    out += QStringLiteral("|2");
    for (int group = 0; group < 12; ++group) {
        const QByteArray key =
            QStringLiteral("buttonbox_other_buttons_bitfield_%1").arg(group).toLatin1();
        out += QLatin1Char('|')
            + QString::number(settingInt(settings, key.constData(), 0));
    }
    // Button order; empty is the identity order.
    QString map;
    const QJsonArray order = settings.value(QStringLiteral("buttonbox_button_map")).toArray();
    if (order.size() == 384) {
        bool identity = true;
        QStringList ids;
        ids.reserve(order.size());
        for (int i = 0; i < order.size(); ++i) {
            const int id = order.at(i).toInt();
            if (id != i) { identity = false; }
            ids.append(QString::number(id));
        }
        if (!identity) { map = ids.join(QLatin1Char(',')); }
    }
    out += QLatin1Char('|') + map;

    QJsonArray macros;
    for (int i = 0; i < 31; ++i) {
        const QJsonObject macro = settings.value(
            QStringLiteral("buttonbox_other_buttons_macro_settings_%1").arg(i)).toObject();
        if (macro.isEmpty()) { continue; }
        const QJsonValue typeValue = macro.value(QStringLiteral("buttonstate_type"));
        const QString typeName = typeValue.isObject()
            ? typeValue.toObject().value(QStringLiteral("name")).toString()
            : typeValue.toString();
        const QString onText = macro.value(QStringLiteral("on_text")).toString();
        const QString offText = macro.value(QStringLiteral("off_text")).toString();
        if (typeName == QLatin1String("OFF") && onText.isEmpty() && offText.isEmpty()) {
            continue;
        }
        int type = 2; // TOGGLE
        if (typeName == QLatin1String("OFF")) { type = 0; }
        else if (typeName == QLatin1String("ON")) { type = 1; }
        else if (typeName == QLatin1String("LED")) { type = 3; }
        else if (typeName == QLatin1String("CONT_VIS")) { type = 4; }
        else if (typeName == QLatin1String("CAT")) { type = 5; }
        const QJsonArray send = macro.value(QStringLiteral("send_via_mmio")).toArray();
        const QJsonArray four = macro.value(QStringLiteral("mmio_4char")).toArray();
        const QJsonArray onMessage = macro.value(QStringLiteral("mmio_message_on")).toArray();
        const QJsonArray offMessage = macro.value(QStringLiteral("mmio_message_off")).toArray();
        const auto firstText = [](const QJsonArray& values) {
            return values.isEmpty() ? QString() : values.at(0).toString();
        };
        macros.append(QJsonObject{
            {QStringLiteral("i"), i},
            {QStringLiteral("t"), type},
            {QStringLiteral("on"), onText},
            {QStringLiteral("off"), offText},
            {QStringLiteral("cat"), macro.value(QStringLiteral("cat_macro")).toString()},
            {QStringLiteral("vis"), macro.value(QStringLiteral("buttonstate_container_visible_id"))
                                        .toString().toInt()},
            {QStringLiteral("send"), !send.isEmpty() && send.at(0).toBool()},
            {QStringLiteral("four"), firstText(four)},
            {QStringLiteral("onMsg"), firstText(onMessage)},
            {QStringLiteral("offMsg"), firstText(offMessage)},
            {QStringLiteral("state"), macro.value(QStringLiteral("on_state")).toBool()},
        });
    }
    out += QStringLiteral("|MACROS:") + QString::fromLatin1(
        QJsonDocument(macros).toJson(QJsonDocument::Compact).toBase64());
    return out;
}

} // namespace

QString ThetisContainerFile::bridgePath()
{
    const QString override = qEnvironmentVariable("NEREUS_THETIS_DAT_BRIDGE");
    if (!override.isEmpty() && QFileInfo::exists(override)) { return override; }
    const QString beside = QCoreApplication::applicationDirPath() + QStringLiteral("/thetis-dat-bridge.exe");
    if (QFileInfo::exists(beside)) { return beside; }
    return {};
}

bool ThetisContainerFile::available() { return !bridgePath().isEmpty(); }

QByteArray ThetisContainerFile::encode(const ContainerDocument& container, QString* error)
{
    const QString id = container.id.isEmpty() ? newId() : container.id;
    const int rxSource = container.config.value(QStringLiteral("rxSource")).toInt(1);
    const QRect geometry = container.geometry.isValid()
        ? container.geometry : QRect(0, 0, 495, 163);

    auto truth = [](bool value) { return value ? QStringLiteral("true") : QStringLiteral("false"); };
    const QStringList fields{
        id,
        QString::number(rxSource),
        QString::number(geometry.x()), QString::number(geometry.y()),
        QString::number(geometry.width()), QString::number(geometry.height()),
        truth(container.dockMode == DockMode::Floating),
        QString::number(container.config.value(QStringLiteral("deltaX")).toInt()),
        QString::number(container.config.value(QStringLiteral("deltaY")).toInt()),
        axisName(container.anchor),
        truth(container.config.value(QStringLiteral("pinOnTop")).toBool()),
        truth(container.config.value(QStringLiteral("border")).toBool()),
        hexToThetisColour(container.config.value(QStringLiteral("backgroundColor")).toString(
            QStringLiteral("#00FFFFFF"))),
        truth(container.config.value(QStringLiteral("noControls")).toBool()),
        truth(container.config.value(QStringLiteral("enabled")).toBool(true)),
        container.name,
        truth(container.config.value(QStringLiteral("containerMinimises")).toBool()),
        truth(container.autoHeight),
        truth(container.config.value(QStringLiteral("showOnRx")).toBool(true)),
        truth(container.config.value(QStringLiteral("showOnTx")).toBool(true)),
        truth(container.locked),
        truth(container.config.value(QStringLiteral("hidesWhenRxNotUsed")).toBool()),
        QStringLiteral("false"), // HiddenByMacro
        truth(container.linkedHfAuto),
        truth(container.linkedWaveNode),
        truth(container.lockItemPositions),
    };

    QJsonObject dictionary;
    dictionary.insert(QStringLiteral("FileVersion"), QStringLiteral("1"));
    dictionary.insert(QStringLiteral("ThetisVersion"), QStringLiteral("NereusSDR"));
    dictionary.insert(QStringLiteral("UTCTDateTime"),
                      QDateTime::currentDateTimeUtc().toString(QStringLiteral("h:mm:ss AP dddd, MMMM d, yyyy")));
    dictionary.insert(QStringLiteral("ContainerData_") + id, fields.join(QLatin1Char('|')));
    dictionary.insert(QStringLiteral("FormData_") + id,
                      QStringLiteral("%1|%2|%3|%4").arg(geometry.x()).arg(geometry.y())
                          .arg(geometry.width()).arg(geometry.height()));
    dictionary.insert(QStringLiteral("MeterData_") + id,
                      QStringLiteral("%1|UCMeter_%1|1|1.0000|1.0000|-1|0.0040|0.0410|0.0500|false|0.0000").arg(id));

    QStringList records;
    for (int index = 0; index < container.contents.size(); ++index) {
        const ContentEntry& entry = container.contents[index];
        QJsonObject record = entryJson(entry);
        record.insert(QStringLiteral("kind"), entry.typeId);
        record.insert(QStringLiteral("nereusEntry"), entryJson(entry));
        record.insert(QStringLiteral("x"), entry.canvasRect.x());
        record.insert(QStringLiteral("y"), entry.canvasRect.y());
        record.insert(QStringLiteral("w"), entry.canvasRect.width());
        record.insert(QStringLiteral("h"), entry.canvasRect.height());
        record.insert(QStringLiteral("z"), entry.paintOrder);
        records.append(QString::fromUtf8(QJsonDocument(record).toJson(QJsonDocument::Compact)));

        const QString groupId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        dictionary.insert(QStringLiteral("ItemGroupData_") + groupId,
                          QStringLiteral("%1|%2|ITEM_GROUP|%3|%4|%5|%6|%7|%8")
                              .arg(groupId, id)
                              .arg(entry.canvasRect.x(), 0, 'f', 4)
                              .arg(entry.canvasRect.y(), 0, 'f', 4)
                              .arg(entry.canvasRect.width(), 0, 'f', 4)
                              .arg(entry.canvasRect.height(), 0, 'f', 4)
                              .arg(thetisMeterTypeFor(entry.typeId))
                              .arg(entry.paintOrder));
        const QString settings = entry.extensions.value(QStringLiteral("thetisItemSettings")).toString();
        if (!settings.isEmpty()) {
            dictionary.insert(QStringLiteral("ItemGroupSettings_2_") + groupId, settings);
        }
    }
    dictionary.insert(QStringLiteral("ContainerItems_") + id, records.join(QLatin1Char('\n')));

    const QString json = QString::fromUtf8(QJsonDocument(dictionary).toJson(QJsonDocument::Compact));
    QByteArray packed;
    if (!runBridge(QStringLiteral("pack"), json.toUtf8(), &packed, error)) { return {}; }
    return packed;
}

DocumentResult ThetisContainerFile::decode(const QByteArray& bytes)
{
    QByteArray json;
    QString error;
    if (!runBridge(QStringLiteral("unpack"), bytes, &json, &error)) {
        return {false, {}, error};
    }
    const QJsonObject dictionary = QJsonDocument::fromJson(json).object();
    QString containerKey;
    for (auto it = dictionary.begin(); it != dictionary.end(); ++it) {
        if (it.key().startsWith(QStringLiteral("ContainerData_"))) { containerKey = it.key(); break; }
    }
    if (containerKey.isEmpty()) {
        return {false, {}, QStringLiteral("The file has no Thetis container data.")};
    }
    const QString id = containerKey.mid(QStringLiteral("ContainerData_").size());
    const QStringList fields = dictionary.value(containerKey).toString().split(QLatin1Char('|'));

    ContainerDocument container;
    container.id = id.isEmpty() ? newId() : id;
    container.name = fields.value(15, QStringLiteral("Thetis container"));
    if (fields.size() >= 6) {
        const QRect geometry(fields[2].toInt(), fields[3].toInt(), fields[4].toInt(), fields[5].toInt());
        if (geometry.isValid()) { container.geometry = geometry; }
    }
    // A floating Thetis layout keeps its window placement in FormData.
    const QString formData = dictionary.value(QStringLiteral("FormData_") + id).toString();
    const QStringList formFields = formData.split(QLatin1Char('|'));
    if (formFields.size() >= 4) {
        const QRect geometry(formFields[0].toInt(), formFields[1].toInt(),
                             formFields[2].toInt(), formFields[3].toInt());
        if (geometry.width() > 0 && geometry.height() > 0) { container.geometry = geometry; }
    }
    // The saved window height sizes the text overlays (see textOverlayRecord).
    // Captions moved under the meters grow the container later, but the
    // fonts were chosen for this window and must not grow with it.
    const int designWindowHeight = qMax(1, container.geometry.height());
    // MeterData field 9 is PixelLayout; field 10 is the item pixel scale.
    bool pixelLayout = false;
    double pixelScale = 0.0;
    const QString meterData = dictionary.value(QStringLiteral("MeterData_") + id).toString();
    const QStringList meterFields = meterData.split(QLatin1Char('|'));
    if (meterFields.size() > 10) {
        pixelLayout = meterFields[9].compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
        pixelScale = meterFields[10].toDouble();
    }
    container.dockMode = fields.value(6).compare(QLatin1String("true"), Qt::CaseInsensitive) == 0
        ? DockMode::Floating : DockMode::OverlayDocked;
    container.anchor = axisFromName(fields.value(9));
    const bool enabled = fields.size() <= 14 || fields[14].compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
    const bool hiddenByMacro = fields.value(22).compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
    container.visible = enabled && !hiddenByMacro;
    container.locked = fields.value(20).compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
    container.lockItemPositions = fields.value(25).compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
    container.linkedHfAuto = fields.value(23).compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
    container.linkedWaveNode = fields.value(24).compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
    container.autoHeight = fields.value(17).compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
    container.config = {
        {QStringLiteral("rxSource"), fields.value(1, QStringLiteral("1")).toInt()},
        {QStringLiteral("sliceId"), fields.value(1, QStringLiteral("1")).toInt() - 1},
        {QStringLiteral("deltaX"), fields.value(7).toInt()},
        {QStringLiteral("deltaY"), fields.value(8).toInt()},
        {QStringLiteral("pinOnTop"), fields.value(10).compare(QLatin1String("true"), Qt::CaseInsensitive) == 0},
        {QStringLiteral("border"), fields.value(11).compare(QLatin1String("true"), Qt::CaseInsensitive) == 0},
        {QStringLiteral("backgroundColor"), thetisColourToHex(fields.value(12))},
        {QStringLiteral("noControls"), fields.value(13).compare(QLatin1String("true"), Qt::CaseInsensitive) == 0},
        {QStringLiteral("containerMinimises"), fields.value(16).compare(QLatin1String("true"), Qt::CaseInsensitive) == 0},
        {QStringLiteral("showOnRx"), fields.size() <= 19 || fields[18].compare(QLatin1String("true"), Qt::CaseInsensitive) == 0},
        {QStringLiteral("showOnTx"), fields.size() <= 19 || fields[19].compare(QLatin1String("true"), Qt::CaseInsensitive) == 0},
        {QStringLiteral("hidesWhenRxNotUsed"), fields.value(21).compare(QLatin1String("true"), Qt::CaseInsensitive) == 0},
    };

    const QString itemPayload = dictionary.value(QStringLiteral("ContainerItems_") + id).toString();
    if (!itemPayload.isEmpty()) {
        const QStringList lines = itemPayload.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        bool allNereus = true;
        for (const QString& line : lines) {
            if (!QJsonDocument::fromJson(line.toUtf8()).object().contains(QStringLiteral("nereusEntry"))) {
                allNereus = false;
                break;
            }
        }
        if (allNereus) {
            for (const QString& line : lines) {
                const QJsonObject record = QJsonDocument::fromJson(line.toUtf8()).object();
                container.contents.append(entryFrom(record.value(QStringLiteral("nereusEntry")).toObject()));
            }
        } else {
            // A NereusSDR legacy payload: reuse the raw-first importer.
            const QByteArray synthetic = dictionary.value(containerKey).toString().toUtf8()
                + '\n' + itemPayload.toUtf8();
            DocumentResult imported = LegacyContainerImporter::fromContainerFile(synthetic);
            if (imported.ok && imported.document.containers.size() == 1) {
                const ContainerDocument legacy = imported.document.containers.first();
                container.contents = legacy.contents;
                container.layout = legacy.layout;
                container.extensions = legacy.extensions;
            }
        }
    } else {
        // Pure Thetis file: item groups carry the normalized placement.
        QStringList keys;
        for (auto it = dictionary.begin(); it != dictionary.end(); ++it) {
            if (it.key().startsWith(QStringLiteral("ItemGroupData_"))) { keys.append(it.key()); }
        }
        std::sort(keys.begin(), keys.end(), [&dictionary](const QString& left, const QString& right) {
            const QStringList a = dictionary.value(left).toString().split(QLatin1Char('|'));
            const QStringList b = dictionary.value(right).toString().split(QLatin1Char('|'));
            return a.value(8).toInt() < b.value(8).toInt();
        });
        // Pass 1: every group's placement, in the order Thetis rebuilds a
        // pixel layout (RebuildPixel's running natural top).
        double pixelTop = 0.0;
        for (const QString& key : keys) {
            const QString raw = dictionary.value(key).toString();
            const QStringList group = raw.split(QLatin1Char('|'));
            if (group.size() < 9) { continue; }
            const QString groupId = group.value(0);
            ContentEntry entry;
            entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            entry.typeId = nereusTypeFor(group.value(7));
            entry.name = group.value(7);
            entry.canvasRect = QRectF(group[3].toDouble(), group[4].toDouble(),
                                      group[5].toDouble(), group[6].toDouble());
            entry.paintOrder = group.value(8).toInt();
            entry.extensions.insert(QStringLiteral("thetisItemGroup"), raw);
            const QString settingsText = dictionary.value(QStringLiteral("ItemGroupSettings_2_") + groupId).toString();
            if (!settingsText.isEmpty()) {
                entry.extensions.insert(QStringLiteral("thetisItemSettings"), settingsText);
                QJsonObject settings;
                if (readThetisSettings(settingsText, &settings)) {
                    entry.extensions.insert(QStringLiteral("thetisSettings"), settings);
                    // Thetis stores the item's placement in the settings, not
                    // only in ItemGroupData: offsets, width scale and pixel
                    // layout. Convert it to the normalized Canvas rect.
                    applyThetisGeometry(entry, settings,
                                        container.geometry.width(), container.geometry.height(),
                                        pixelLayout, pixelScale, &pixelTop);
                }
            }
            container.contents.append(entry);
        }

        // WaveNode's saved layout puts its two amplifier captions over the
        // bottom of their cross meters. Show them under the meters instead,
        // centred on them, with the names the station uses.
        if (pixelLayout && container.linkedWaveNode) {
            QRectF leftMeter;
            QRectF rightMeter;
            bool haveLeft = false;
            bool haveRight = false;
            for (const auto& entry : container.contents) {
                if (entry.typeId != QLatin1String("meter.crossNeedle")) { continue; }
                if (entry.canvasRect.center().x() < 0.5) {
                    leftMeter = entry.canvasRect;
                    haveLeft = true;
                } else {
                    rightMeter = entry.canvasRect;
                    haveRight = true;
                }
            }
            const double oldHeight = qMax(1, container.geometry.height());
            const double gap = 2.0 / oldHeight;
            for (auto& entry : container.contents) {
                if (entry.typeId != QLatin1String("TEXTOVERLAY")) { continue; }
                QJsonObject settings =
                    entry.extensions.value(QStringLiteral("thetisSettings")).toObject();
                const QString text = settingString(settings, "_text_1");
                QString replacement;
                if (text == QLatin1String("Anvelina")) { replacement = QStringLiteral("Amp-In"); }
                else if (text == QLatin1String("Amplifier")) { replacement = QStringLiteral("Amp-Out"); }
                if (replacement.isEmpty()) { continue; }
                settings.insert(QStringLiteral("_text_1"), replacement);
                settings.insert(QStringLiteral("_text_2"), replacement);
                entry.extensions.insert(QStringLiteral("thetisSettings"), settings);
                entry.name = replacement;
                const bool left = entry.canvasRect.center().x() < 0.5;
                if (left ? haveLeft : haveRight) {
                    const QRectF& meter = left ? leftMeter : rightMeter;
                    // Text overlays draw left-aligned. Offset the (still
                    // meter-wide) box so the caption's own width lands in
                    // the middle of the dial.
                    const double font = qMin(16.0,
                        settingDouble(settings, "_font_size_1", 16.0));
                    const double textWidth =
                        replacement.size() * font * 2.1 * 0.55 / 832.0;
                    entry.canvasRect.setLeft(meter.center().x() - textWidth / 2.0);
                    entry.canvasRect.setWidth(meter.width());
                    // The meter's saved rect includes transparent space below
                    // its readout. Put the caption just clear of that readout.
                    entry.canvasRect.moveTop(meter.bottom() - 6.0 / oldHeight);
                }
            }
            // Make room under the meters: everything below them (the temp
            // bars and the amplifier buttons) moves down by the caption
            // depth. The window grows with it, so nothing is squeezed.
            const double meterBottom = qMax(haveLeft ? leftMeter.bottom() : 0.0,
                                            haveRight ? rightMeter.bottom() : 0.0);
            QStringList fixed;
            double captionBottom = 0.0;
            for (const auto& entry : container.contents) {
                if (entry.typeId == QLatin1String("meter.crossNeedle")) {
                    fixed.append(entry.id);
                } else if (entry.name == QLatin1String("Amp-In")
                           || entry.name == QLatin1String("Amp-Out")) {
                    fixed.append(entry.id);
                    captionBottom = qMax(captionBottom, entry.canvasRect.bottom());
                }
            }
            double moveDelta = 0.0;
            double firstBelow = std::numeric_limits<double>::max();
            for (const auto& entry : container.contents) {
                if (fixed.contains(entry.id)) { continue; }
                // Only the columns under the meters move; the centre column
                // keeps its label/value rows together.
                const double centreX = entry.canvasRect.center().x();
                const bool sideColumn = centreX < 0.34 || centreX > 0.65;
                if (sideColumn && entry.canvasRect.top() >= meterBottom - 0.001) {
                    firstBelow = qMin(firstBelow, entry.canvasRect.top());
                }
            }
            if (firstBelow < std::numeric_limits<double>::max() && captionBottom > 0.0) {
                moveDelta = qMax(0.0, captionBottom + gap - firstBelow);
            }
            if (moveDelta > 0.0) {
                for (auto& entry : container.contents) {
                    if (fixed.contains(entry.id)) { continue; }
                    const double centreX = entry.canvasRect.center().x();
                    const bool sideColumn = centreX < 0.34 || centreX > 0.65;
                    if (sideColumn && entry.canvasRect.top() >= meterBottom - 0.001) {
                        entry.canvasRect.moveTop(entry.canvasRect.top() + moveDelta);
                    }
                }
            }

            // The two temperature bars share the right column in the saved
            // layout and read as one crowded stack. Put them side by side
            // under the Amp-Out caption instead.
            QVector<ContentEntry*> tempBars;
            for (auto& entry : container.contents) {
                if (entry.typeId != QLatin1String("meter.customBar")) { continue; }
                const QJsonObject barSettings =
                    entry.extensions.value(QStringLiteral("thetisSettings")).toObject();
                const QString title = settingString(barSettings, "meter_custom_title");
                if (title.contains(QLatin1String("Temp"), Qt::CaseInsensitive)) {
                    tempBars.append(&entry);
                }
            }
            if (tempBars.size() == 2 && haveRight) {
                std::sort(tempBars.begin(), tempBars.end(),
                          [](const ContentEntry* left, const ContentEntry* right) {
                              return left->extensions
                                         .value(QStringLiteral("thetisSettings"))
                                         .toObject()
                                         .value(QStringLiteral("meter_custom_title"))
                                         .toString()
                                   < right->extensions
                                         .value(QStringLiteral("thetisSettings"))
                                         .toObject()
                                         .value(QStringLiteral("meter_custom_title"))
                                         .toString();
                          });
                const double windowW = container.geometry.width();
                const double gapPx = 10.0;
                // Use the empty band under the centre column and the right
                // meter, clear of the amplifier buttons on the left. Wider
                // cells keep the bar's title readable.
                const double rowLeft = 0.35;
                const double rowRight = 1.0;
                const double barWidthPx = qMax(40.0,
                    ((rowRight - rowLeft) * windowW - gapPx) / 2.0);
                // Below the Amp-Out caption's whole panel: its background is
                // opaque and paints over anything placed inside it.
                const double barY = captionBottom + 6.0 / oldHeight;
                const double barHeight = 40.0 / oldHeight;
                QVector<ContentEntry> barLabels;
                for (int i = 0; i < tempBars.size(); ++i) {
                    // The centre column's text panels are tall and were
                    // painting over the left bar's title. A larger layer is
                    // in front of them.
                    tempBars[i]->paintOrder = 200;
                    tempBars[i]->canvasRect.moveTop(barY);
                    tempBars[i]->canvasRect.setHeight(barHeight);
                    tempBars[i]->canvasRect.setLeft(
                        rowLeft + i * (barWidthPx + gapPx) / windowW);
                    tempBars[i]->canvasRect.setWidth(barWidthPx / windowW);

                    // The bar's own title area is too narrow when two bars
                    // share the row; give each one a small text label of its
                    // own (the bar record's title is turned off below).
                    const QJsonObject barSettings =
                        tempBars[i]->extensions.value(QStringLiteral("thetisSettings")).toObject();
                    const QString title = settingString(barSettings, "meter_custom_title");
                    if (title.isEmpty()) { continue; }
                    constexpr double kBarLabelFont = 7.0;
                    const double textWidth =
                        title.size() * kBarLabelFont * 2.1 * 0.55 / 832.0;
                    ContentEntry label;
                    label.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
                    label.typeId = QStringLiteral("TEXTOVERLAY");
                    label.name = title;
                    label.paintOrder = 210;
                    label.canvasRect = QRectF(
                        tempBars[i]->canvasRect.center().x() - textWidth / 2.0,
                        barY - 9.0 / oldHeight,
                        qMax(0.12, textWidth + 0.02), 12.0 / oldHeight);
                    QJsonObject textSettings;
                    textSettings.insert(QStringLiteral("_text_1"), title);
                    textSettings.insert(QStringLiteral("_text_2"), title);
                    textSettings.insert(QStringLiteral("_font_size_1"), kBarLabelFont);
                    textSettings.insert(QStringLiteral("_font_size_2"), kBarLabelFont);
                    textSettings.insert(QStringLiteral("_font_family_1"),
                                        QStringLiteral("Trebuchet MS"));
                    textSettings.insert(QStringLiteral("_colour"),
                                        barSettings.value(QStringLiteral("_titleColor"))
                                            .toString(QStringLiteral("#FFFFFFFF")));
                    textSettings.insert(QStringLiteral("_titleColor"),
                                        QStringLiteral("#FF000000"));
                    textSettings.insert(QStringLiteral("_showSubMarker"), false);
                    textSettings.insert(QStringLiteral("_showMarker"), false);
                    label.extensions.insert(QStringLiteral("thetisSettings"), textSettings);
                    label.config.insert(QStringLiteral("thetisMaxFont"), kBarLabelFont);
                    barLabels.append(label);
                }
                for (const ContentEntry& label : barLabels) {
                    container.contents.append(label);
                }

                // Give the amplifier button row the full width of the left
                // meter. The saved box is narrower and its first caption
                // touches the edge.
                if (haveLeft) {
                    for (auto& entry : container.contents) {
                        if (entry.typeId != QLatin1String("OTHERBTNS")) { continue; }
                        const QJsonObject buttonSettings =
                            entry.extensions.value(QStringLiteral("thetisSettings")).toObject();
                        if (settingInt(buttonSettings,
                                       "buttonbox_other_buttons_bitfield_11", 0) & 0x0F) {
                            entry.canvasRect.setLeft(leftMeter.left());
                            entry.canvasRect.setWidth(leftMeter.width());
                        }
                    }
                }
            }

            // Grow the design window downwards for the moved captions, so
            // fitting them does not shrink the meters.
            bool haveBounds = false;
            QRectF bounds;
            for (const auto& entry : container.contents) {
                if (!entry.visible) { continue; }
                const QRectF rect(entry.canvasRect.x() * container.geometry.width(),
                                  entry.canvasRect.y() * oldHeight,
                                  entry.canvasRect.width() * container.geometry.width(),
                                  entry.canvasRect.height() * oldHeight);
                bounds = haveBounds ? bounds.united(rect) : rect;
                haveBounds = true;
            }
            if (haveBounds) {
                const double required = bounds.bottom() + 2.0;
                if (required > oldHeight + 1.0) {
                    // Canvas y is a fraction of the window height, so the
                    // same design pixels become a smaller fraction in the
                    // taller window.
                    const double factor = oldHeight / required;
                    container.geometry.setHeight(qRound(required));
                    for (auto& entry : container.contents) {
                        entry.canvasRect.setY(entry.canvasRect.y() * factor);
                        entry.canvasRect.setHeight(entry.canvasRect.height() * factor);
                    }
                }
            }
        }

        // Pixel layouts are designed on a fixed reference and often put the
        // last rows just past the saved window. Fit the used extent into the
        // window as one piece (uniform scale, centred) so nothing is cut off
        // and the items keep their shape. The old per-axis fit stretched
        // text and meters; this one does not.
        if (pixelLayout && !container.contents.isEmpty()) {
            const double windowW = qMax(1, container.geometry.width());
            const double windowH = qMax(1, container.geometry.height());
            bool haveBounds = false;
            QRectF bounds;
            for (const auto& entry : container.contents) {
                if (!entry.visible) { continue; }
                // The amplifier captions sit centred on their dials but keep
                // a full dial-wide box, which extends past the glyphs. They
                // must not drag the fitted centre sideways.
                if (entry.name == QLatin1String("Amp-In")
                    || entry.name == QLatin1String("Amp-Out")) {
                    continue;
                }
                const QRectF rect(entry.canvasRect.x() * windowW,
                                  entry.canvasRect.y() * windowH,
                                  entry.canvasRect.width() * windowW,
                                  entry.canvasRect.height() * windowH);
                bounds = haveBounds ? bounds.united(rect) : rect;
                haveBounds = true;
            }
            if (haveBounds && bounds.width() > 0.0 && bounds.height() > 0.0) {
                const double scale = qMin(1.0,
                    qMin(windowW / bounds.width(), windowH / bounds.height()));
                if (scale < 0.999) {
                    // Centre left to right, but keep the face anchored to
                    // the top of the window.
                    const double offsetX = (windowW - bounds.width() * scale) / 2.0;
                    const double offsetY = 0.0;
                    for (auto& entry : container.contents) {
                        const double x = entry.canvasRect.x() * windowW;
                        const double y = entry.canvasRect.y() * windowH;
                        entry.canvasRect = QRectF(
                            (offsetX + (x - bounds.left()) * scale) / windowW,
                            (offsetY + (y - bounds.top()) * scale) / windowH,
                            entry.canvasRect.width() * scale,
                            entry.canvasRect.height() * scale);
                    }
                }
            }
        }

        // Pass 2: the per-type records, built from the fitted rects.
        for (auto& entry : container.contents) {
            QJsonObject settings =
                entry.extensions.value(QStringLiteral("thetisSettings")).toObject();
            if (settings.isEmpty()) { continue; }
            if (entry.typeId == QLatin1String("TEXTOVERLAY")) {
                // The WaveNode centre column is narrow: the saved 18-point
                // font overruns its 187-pixel row and clips the words. Keep
                // those rows inside the column; the meter captions below
                // the dials keep their larger size.
                if (container.linkedWaveNode) {
                    const double centreX = entry.canvasRect.center().x();
                    double fontCap = 0.0;
                    if (entry.name == QLatin1String("Amp-In")
                        || entry.name == QLatin1String("Amp-Out")) {
                        // The mounted amplifier captions: a little smaller
                        // than Thetis's 23.5-point save, so they sit under
                        // the dials instead of dominating the window.
                        fontCap = 16.0;
                    } else if (centreX >= 0.34 && centreX <= 0.65) {
                        fontCap = 10.0;
                    }
                    if (fontCap > 0.0) {
                        for (const char* key : { "_font_size_1", "_font_size_2" }) {
                            const double value = settingDouble(settings, key, fontCap);
                            settings.insert(QLatin1String(key), qMin(value, fontCap));
                        }
                    }
                }
                const QString overlayText = settingString(settings, "_text_1");
                // The HF-Auto C/L row carries the blinking selection
                // window. Paint its text above that window and let
                // the olive container background show through, so
                // the selected value stays readable.
                const bool clRow = overlayText.contains(QLatin1String("C="))
                    && overlayText.contains(QLatin1String("L="));
                if (clRow) { entry.paintOrder = 100; }
                const bool caption = entry.name == QLatin1String("Amp-In")
                    || entry.name == QLatin1String("Amp-Out");
                const double maxFontOverride =
                    entry.config.value(QStringLiteral("thetisMaxFont")).toDouble(0.0);
                entry.config.insert(QStringLiteral("legacyRecord"),
                                    textOverlayRecord(entry, settings,
                                                      container.geometry.width(),
                                                      designWindowHeight,
                                                      clRow || caption,
                                                      maxFontOverride));
                // The overlay paints its own text; a name made from
                // that text would draw a second copy in menus and
                // headers. Leave the generic type name.
                entry.name = QStringLiteral("Text overlay");
            } else if (entry.typeId == QLatin1String("LED")) {
                applyLedSettings(entry, settings,
                                 container.geometry.width(),
                                 container.geometry.height());
                entry.config.insert(QStringLiteral("legacyRecord"),
                                    ledRecord(entry, settings));
            } else if (entry.typeId == QLatin1String("meter.crossNeedle")) {
                applyCrossSettings(entry, settings);
            } else if (entry.typeId == QLatin1String("meter.customBar")) {
                if (container.linkedWaveNode) {
                    // One reading per temperature bar; the saved peak value
                    // drew a second "0.0 C" and narrowed the title.
                    settings.insert(QStringLiteral("_peakValue"), false);
                }
                applyCustomBarSettings(entry, settings, !container.linkedWaveNode);
            } else if (entry.typeId == QLatin1String("OTHERBTNS")) {
                entry.config.insert(QStringLiteral("legacyRecord"),
                                    otherButtonsRecord(entry, settings));
                entry.visible = true;
            }
        }
        // The HF-Auto rows are saved against a tall window and use only the
        // top third when the helper is shown in its fitted window. Centre
        // the visible text block left to right; the rows stay at the top.
        if (container.linkedHfAuto && !pixelLayout && !container.contents.isEmpty()) {
            double blockWidth = 0.0;
            for (const auto& entry : container.contents) {
                if (!entry.visible || entry.typeId != QLatin1String("TEXTOVERLAY")) {
                    continue;
                }
                const QJsonObject textSettings =
                    entry.extensions.value(QStringLiteral("thetisSettings")).toObject();
                QString text = settingString(textSettings, "_text_1");
                // A token's displayed value is a few characters, not the
                // whole "%QxG4.name%" reference.
                QString expanded;
                for (int i = 0; i < text.size(); ++i) {
                    if (text[i] == QLatin1Char('%')) {
                        const int end = text.indexOf(QLatin1Char('%'), i + 1);
                        if (end > i + 1) {
                            expanded += QStringLiteral("0000");
                            i = end;
                            continue;
                        }
                    }
                    expanded += text[i];
                }
                const double fontSize = settingDouble(textSettings, "_font_size_1", 18.0);
                // Same effective font the overlay draws: saved points times
                // the Thetis fudge, scaled to the imported window width.
                QFont font(settingString(textSettings, "_font_family_1",
                                         QStringLiteral("Trebuchet MS")));
                font.setPixelSize(qMax(1, qRound(fontSize * 2.1
                                                 * container.geometry.width() / 832.0)));
                const QFontMetrics metrics(font);
                const double width =
                    metrics.horizontalAdvance(expanded) / double(container.geometry.width());
                blockWidth = qMax(blockWidth, width);
            }
            if (blockWidth > 0.0) {
                // Left-to-right only: the HF-Auto rows stay anchored to the
                // top of the container.
                const double offsetX = qMax(0.0, (1.0 - blockWidth) / 2.0);
                for (auto& entry : container.contents) {
                    entry.canvasRect.translate(offsetX, 0.0);
                    QString record =
                        entry.config.value(QStringLiteral("legacyRecord")).toString();
                    if (record.isEmpty()) { continue; }
                    if (record.startsWith(QLatin1Char('{'))) {
                        QJsonObject object =
                            QJsonDocument::fromJson(record.toUtf8()).object();
                        if (!object.isEmpty()) {
                            object.insert(QStringLiteral("x"), entry.canvasRect.x());
                            record = QString::fromUtf8(
                                QJsonDocument(object).toJson(QJsonDocument::Compact));
                        }
                    } else {
                        QStringList parts = record.split(QLatin1Char('|'));
                        if (parts.size() > 4) {
                            parts[1] = QString::number(entry.canvasRect.x());
                            record = parts.join(QLatin1Char('|'));
                        }
                    }
                    entry.config.insert(QStringLiteral("legacyRecord"), record);
                }
            }
        }

        container.layout = ContentLayout::LegacyCanvas;
    }

    // A transparent Thetis container backdrop takes the colour of its first
    // filled text panel, so the unused part of the window reads as the same
    // panel instead of the host window's white.
    QColor backdrop(container.config.value(QStringLiteral("backgroundColor")).toString());
    if (backdrop.alpha() == 0) {
        for (const auto& entry : container.contents) {
            if (entry.typeId != QLatin1String("TEXTOVERLAY")) { continue; }
            const QJsonObject settings =
                entry.extensions.value(QStringLiteral("thetisSettings")).toObject();
            if (!settingBool(settings, "_showSubMarker", false)) { continue; }
            QColor panel(colourSetting(settings, "_titleColor", QStringLiteral("#FF202020")));
            if (panel.isValid() && panel.alpha() > 0) {
                panel.setAlpha(255);
                backdrop = panel;
                break;
            }
        }
        if (backdrop.alpha() > 0) {
            container.config[QStringLiteral("backgroundColor")] =
                backdrop.name(QColor::HexArgb);
        }
    }

    WorkspaceDocument document;
    document.mainContainerId = container.id;
    document.containers.append(container);
    const QString validation = ContainerDocumentCodec::validate(document);
    if (!validation.isEmpty()) { return {false, {}, validation}; }
    return {true, document, {}};
}

} // namespace NereusSDR
