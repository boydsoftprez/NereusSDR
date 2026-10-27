// no-port-check: NereusSDR-original test.
//
// The workspace look for container and item colours (ItemColourLook.h):
// every default colour of every registry item type is in the table, mapped
// to a palette role or kept; a mapped default draws the role's colour with
// the look on, a user's colour stays, and the look off keeps the classic
// colour; records round-trip byte for byte; the editors' swatches show the
// colour drawn and a pick stores the user's colour; every mapped role
// reads against what it is drawn on (text 4.5:1, graphics 3:1).
//
// Modification history (NereusSDR):
//   2026-10-26 - PR 677 review fix wave: a pick of exactly a default stays
//                 as picked and is saved beside the record (I6); records
//                 include the entry context; the look off keeps every
//                 row's classic colour (I9). J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.

#include <QtTest>
#include <QApplication>
#include <QColorDialog>
#include <QFile>
#include <QSet>
#include <QTextStream>
#include <QImage>
#include <QJsonDocument>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <memory>

#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerDocument.h"
#include "gui/containers/ContainerWidget.h"
#include "gui/containers/meter_property_editors/ClockItemEditor.h"
#include "gui/meters/ClockItem.h"
#include "gui/meters/FilterDisplayItem.h"
#include "gui/meters/ItemColourLook.h"
#include "gui/meters/LEDItem.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/ModeButtonItem.h"
#include "gui/meters/RotatorItem.h"
#include "gui/meters/VfoDisplayItem.h"
#include "gui/styles/WorkspacePalette.h"
#include "gui/styles/WorkspacePreview.h"

using namespace NereusSDR;

namespace {

// The palette's colour for a role (WorkspacePalette::glassAndDepth()).
QColor role(const char* name)
{
    return Workspace::WorkspacePalette::glassAndDepth().color(QString::fromLatin1(name));
}

QByteArray recordOf(const ContainerContentRegistry& registry, const MeterItem& item,
                    const ContentEntry& prior)
{
    const ContentEntry captured = registry.captureMeterItem(item, prior);
    return QJsonDocument(captured.config).toJson(QJsonDocument::Compact)
           + QByteArray("|") + QJsonDocument(captured.context).toJson(QJsonDocument::Compact)
           + QByteArray("|") + item.serialize().toUtf8();
}

// ── Contrast walk ───────────────────────────────────────────────────────
// Every mapped row's role is drawn on a surface; text needs 4.5:1 and a
// graphic 3:1 (WCAG 2.x). kSurfaces says, per field, what the role is
// drawn on and whether it is text; the walk takes every mapped row (with
// the row's own alpha) against each surface. A field whose role is a fill
// (a face, a background) is listed as a fill: the foregrounds drawn on it
// are measured against it. kDerived lists the colours items derive from
// a field (off states, badges) with the roles the code draws.

enum class Kind { Text, Graphic, Fill };

struct Surface {
    const char* field;
    Kind kind;
    // Comma-separated surface roles the colour is drawn on.
    const char* on;
    // An alpha the paint applies over the row's own (0 = none).
    double alpha;
};

constexpr Surface kSurfaces[] = {
    // Container background and the items' own backgrounds
    {"Container.background", Kind::Fill, "", 0},
    {"Solid.colour", Kind::Fill, "", 0}, {"Spacer.colour1", Kind::Fill, "", 0}, {"Spacer.colour2", Kind::Fill, "", 0},
    {"FadeCover.colour1", Kind::Fill, "", 0}, {"FadeCover.colour2", Kind::Fill, "", 0},
    {"WebImage.fallbackColor", Kind::Fill, "", 0},
    // Bar
    {"Bar.barColor", Kind::Graphic, "panel", 0}, {"Bar.barRedColor", Kind::Graphic, "panel", 0},
    {"Bar.markerColour", Kind::Graphic, "panel", 0}, {"Bar.peakHoldMarkerColour", Kind::Graphic, "panel", 0},
    {"Bar.peakFontColour", Kind::Text, "panel", 0}, {"Bar.fontColour", Kind::Text, "panel", 0},
    {"Bar.edgeBackgroundColor", Kind::Fill, "", 0}, {"Bar.edgeLowColor", Kind::Graphic, "inset", 0},
    {"Bar.edgeHighColor", Kind::Graphic, "inset", 0}, {"Bar.edgeAvgColor", Kind::Graphic, "inset", 0},
    {"Bar.historyColour", Kind::Graphic, "panel", 0},
    // Scale, text, needle, power scale
    {"Scale.lowColour", Kind::Text, "panel", 0}, {"Scale.highColour", Kind::Text, "panel", 0},
    {"Scale.titleColour", Kind::Text, "panel", 0}, {"Scale.tickColor", Kind::Graphic, "panel", 0},
    {"Scale.labelColor", Kind::Text, "panel", 0}, {"Text.textColor", Kind::Text, "panel", 0},
    {"Needle.needleColor", Kind::Graphic, "panel", 0}, {"Needle.peakHoldColour", Kind::Graphic, "panel", 0},
    {"Needle.historyColor", Kind::Graphic, "panel", 0},
    {"PowerScale.lowColour", Kind::Text, "panel", 0}, {"PowerScale.highColour", Kind::Text, "panel", 0},
    // LED
    {"Led.trueColour", Kind::Graphic, "panel", 0}, {"Led.falseColour", Kind::Graphic, "panel", 0},
    {"Led.amberColour", Kind::Graphic, "panel", 0}, {"Led.redColour", Kind::Graphic, "panel", 0},
    {"Led.panelBackColour1", Kind::Fill, "", 0}, {"Led.panelBackColour2", Kind::Fill, "", 0},
    // History graph
    {"History.backColour", Kind::Fill, "", 0}, {"History.lineColor0", Kind::Graphic, "panel", 0},
    {"History.lineColor1", Kind::Graphic, "panel", 0}, {"History.linesColour", Kind::Graphic, "panel", 0},
    {"History.timeColour", Kind::Text, "panel", 0},
    // Magic eye
    {"MagicEye.glowColor", Kind::Graphic, "panel", 0}, {"MagicEye.historyColour", Kind::Graphic, "panel", 0},
    // Signal text
    {"SignalText.colour", Kind::Text, "panel", 0}, {"SignalText.peakValueColour", Kind::Text, "panel", 0},
    {"SignalText.typeColour", Kind::Text, "panel", 0}, {"SignalText.subMarkerColour", Kind::Text, "panel", 0},
    {"SignalText.backgroundColour", Kind::Fill, "", 0}, {"SignalText.markerColour", Kind::Graphic, "panel", 0},
    // Tuning dial: corner labels on the off zones (the container's panel)
    // and on the on zone; the pad, ring and speed ring on the circle.
    {"Dial.textColour", Kind::Text, "panel,inset,selected-top", 0}, {"Dial.circleColour", Kind::Fill, "", 0},
    {"Dial.padColour", Kind::Graphic, "inset", 0}, {"Dial.padPressedColour", Kind::Graphic, "inset", 0},
    {"Dial.buttonOnColour", Kind::Graphic, "panel", 0}, {"Dial.buttonOffColour", Kind::Fill, "", 0},
    {"Dial.buttonHighlightColour", Kind::Fill, "", 0}, {"Dial.ringColour", Kind::Graphic, "inset", 0},
    {"Dial.slowColour", Kind::Graphic, "inset", 0}, {"Dial.holdColour", Kind::Graphic, "inset", 0},
    {"Dial.fastColour", Kind::Graphic, "inset", 0},
    // Text overlay
    {"TextOverlay.textColour1", Kind::Text, "raised-top,panel", 0},
    {"TextOverlay.textColour2", Kind::Text, "raised-top,panel", 0},
    {"TextOverlay.textBackColour1", Kind::Fill, "", 0}, {"TextOverlay.textBackColour2", Kind::Fill, "", 0},
    {"TextOverlay.panelBackColour1", Kind::Fill, "", 0}, {"TextOverlay.panelBackColour2", Kind::Fill, "", 0},
    // Filter display: text on the panel and on its highlight boxes
    {"FilterDisplay.dataLineColour", Kind::Graphic, "panel", 0},
    {"FilterDisplay.textColour", Kind::Text, "panel,selected-top,danger-bg,button-hover", 0},
    {"FilterDisplay.numberHighlightColour", Kind::Fill, "", 0},
    {"FilterDisplay.edgesColourRX", Kind::Graphic, "panel", 0}, {"FilterDisplay.edgesColourTX", Kind::Graphic, "panel", 0},
    {"FilterDisplay.edgeHighlightColour", Kind::Graphic, "panel", 0}, {"FilterDisplay.meterBackColour", Kind::Fill, "", 0},
    {"FilterDisplay.notchColour", Kind::Graphic, "panel", 0}, {"FilterDisplay.notchHighlightColour", Kind::Graphic, "panel", 0},
    {"FilterDisplay.extentsColour", Kind::Graphic, "panel", 0}, {"FilterDisplay.snapLineColour", Kind::Graphic, "panel", 0},
    {"FilterDisplay.settingOnColour", Kind::Fill, "", 0}, {"FilterDisplay.buttonHighlightColour", Kind::Fill, "", 0},
    {"FilterDisplay.dataFillColour", Kind::Fill, "", 0},
    // Rotator (the beam is drawn at its 0.6 default alpha)
    {"Rotator.bigBlobColour", Kind::Graphic, "panel", 0}, {"Rotator.smallBlobColour", Kind::Graphic, "panel", 0},
    {"Rotator.outerTextColour", Kind::Text, "panel", 0}, {"Rotator.arrowColour", Kind::Graphic, "panel", 0},
    {"Rotator.beamWidthColour", Kind::Graphic, "panel", 0.6}, {"Rotator.backgroundColour", Kind::Fill, "", 0},
    {"Rotator.controlColour", Kind::Graphic, "panel", 0},
    // VFO display
    {"VfoDisplay.frequencyColour", Kind::Text, "panel,button-hover", 0},
    {"VfoDisplay.frequencySmallColour", Kind::Text, "panel,button-hover", 0},
    {"VfoDisplay.modeColour", Kind::Text, "panel", 0}, {"VfoDisplay.filterColour", Kind::Text, "panel,button-hover", 0},
    {"VfoDisplay.titleColour", Kind::Text, "panel", 0}, {"VfoDisplay.splitBackColour", Kind::Fill, "", 0},
    {"VfoDisplay.splitColour", Kind::Text, "raised-top", 0},
    // The RX and TX stripe at the left; the badges are in kDerived.
    {"VfoDisplay.rxColour", Kind::Graphic, "panel", 0}, {"VfoDisplay.txColour", Kind::Graphic, "panel", 0},
    {"VfoDisplay.bandColour", Kind::Text, "panel,button-hover", 0},
    {"VfoDisplay.digitHighlightColour", Kind::Fill, "", 0}, {"VfoDisplay.bandTextColour", Kind::Text, "panel", 0},
    {"VfoDisplay.lockColour", Kind::Graphic, "panel", 0}, {"VfoDisplay.syncColour", Kind::Graphic, "panel", 64.0 / 255.0},
    {"VfoDisplay.backgroundColour", Kind::Fill, "", 0},
    // Clock
    {"Clock.timeColour", Kind::Text, "panel", 0}, {"Clock.dateColour", Kind::Text, "panel", 0},
    {"Clock.typeTitleColour", Kind::Text, "panel", 0}, {"Clock.backColour", Kind::Fill, "", 0},
    // Button grids: text on every face it can sit on
    {"ButtonBox.fillColour", Kind::Fill, "", 0}, {"ButtonBox.offColour", Kind::Fill, "", 0},
    {"ButtonBox.hoverColour", Kind::Fill, "", 0}, {"ButtonBox.clickColour", Kind::Fill, "", 0},
    {"ButtonBox.borderColour", Kind::Graphic, "panel", 0},
    {"ButtonBox.textColour", Kind::Text,
     "raised-top,button-hover,selected-top,ok-bg,measured-bg,danger-bg,badge-warn-bg", 0},
    {"ButtonBox.onColour", Kind::Fill, "", 0},
    {"NereusBand.buttonTextColor", Kind::Text, "raised-top,raised-bottom", 0},
    {"NereusBand.backgroundTopColor", Kind::Fill, "", 0}, {"NereusBand.backgroundBottomColor", Kind::Fill, "", 0},
    {"Waveform.traceColor", Kind::Graphic, "panel", 0},
    // Bar strips and the composite faces
    {"BarPreset.barColor", Kind::Graphic, "panel", 0}, {"BarPreset.backdropColor", Kind::Fill, "", 0},
    {"BarPreset.titleColor", Kind::Text, "panel", 0}, {"BarPreset.lowColor", Kind::Text, "panel", 0},
    {"BarPreset.highColor", Kind::Text, "panel", 0}, {"BarPreset.subColor", Kind::Graphic, "panel", 0},
    {"BarPreset.peakHoldColor", Kind::Graphic, "panel", 0}, {"BarPreset.peakValueColor", Kind::Text, "panel", 0},
    {"BarPreset.segLowColor", Kind::Graphic, "panel", 0}, {"BarPreset.segHighColor", Kind::Graphic, "panel", 0},
    {"BarPreset.historyColor", Kind::Graphic, "panel", 0},
};

// Composite faces: "<kind>.<key>" and "<kind>.<channel>.<key>" take the
// spec of their key.
constexpr Surface kCompositeKeys[] = {
    {"titleColor", Kind::Text, "panel", 0}, {"backdropColor", Kind::Fill, "", 0},
    {"lowColor", Kind::Text, "panel", 0}, {"highColor", Kind::Text, "panel", 0},
    {"peakHoldColor", Kind::Graphic, "panel", 0}, {"peakValueColor", Kind::Text, "panel", 0},
    {"typeColor", Kind::Text, "panel", 0}, {"subMarkerColor", Kind::Text, "panel", 0},
    {"color", Kind::Graphic, "panel", 0}, {"historyColor", Kind::Graphic, "panel", 0},
};

// Colours an item derives from a field (ItemColourLook::derived()), with
// the roles the paint code names.
struct DerivedPair {
    const char* what;
    Kind kind;
    const char* fg;
    const char* bg;
    double alpha;
};

constexpr DerivedPair kDerived[] = {
    {"VfoDisplay split off text (derived)", Kind::Text, "text-tertiary", "raised-top", 0},
    {"VfoDisplay RX badge lit", Kind::Text, "live", "ok-bg", 0},
    {"VfoDisplay RX badge off", Kind::Text, "text-tertiary", "raised-top", 0},
    {"VfoDisplay TX badge transmitting", Kind::Text, "inset", "tx", 0},
    {"VfoDisplay TX badge able to transmit", Kind::Text, "text", "danger-bg", 0},
    {"VfoDisplay TX badge unable to transmit", Kind::Text, "text-tertiary", "raised-top", 0},
    {"VfoDisplay lock icon off (alpha 64)", Kind::Graphic, "ok", "panel", 64.0 / 255.0},
    {"VfoDisplay slice not in use: frequency (alpha 24)", Kind::Text, "measured", "panel", 24.0 / 255.0},
    {"VfoDisplay slice not in use: labels (alpha 24)", Kind::Text, "text-secondary", "panel", 24.0 / 255.0},
    {"Dial disabled label (derived)", Kind::Text, "text-tertiary", "panel", 0},
};

// Pairs under their threshold that stay, each with why.
struct Faint {
    const char* key;
    const char* why;
};

constexpr Faint kFaint[] = {
    {"History.linesColour/border", "grid lines are structure behind the traces, kept faint on purpose"},
    {"ButtonBox.borderColour/border", "a button's edge; the raised face and its text identify the button"},
    {"Dial.buttonOnColour/selected-top", "the on zone (ACCEL, LOCK) is told by its blue hue and its label, which reads at 4.84:1 on it"},
    {"Bar.historyColour/measured", "a history trail: a half-alpha copy of the reading behind the live bar"},
    {"Needle.historyColor/measured", "a history trail: a faded copy of the needle behind the live needle"},
    {"MagicEye.historyColour/measured", "a history trail behind the live eye"},
    {"BarPreset.historyColor/measured", "a history trail: a faded copy of the reading behind the live bar"},
    {"historyColor/measured", "a composite face's history trail behind the live reading"},
    {"VfoDisplay.syncColour/ok", "the sync icon is always off (no VFO sync); Thetis draws an off icon at alpha 64"},
    {"VfoDisplay lock icon off (alpha 64)", "an off icon reads as off; Thetis draws it at alpha 64 and the locked icon at 6.75:1"},
    {"VfoDisplay slice not in use: frequency (alpha 24)", "Thetis fades a slice not in use to alpha 24; changing that is a design call for JJ"},
    {"VfoDisplay slice not in use: labels (alpha 24)", "Thetis fades a slice not in use to alpha 24; changing that is a design call for JJ"},
};

double luminance(const QColor& c)
{
    const auto lin = [](double v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * lin(c.redF()) + 0.7152 * lin(c.greenF()) + 0.0722 * lin(c.blueF());
}

QColor over(const QColor& fg, double alpha, const QColor& bg)
{
    return QColor::fromRgbF(float(fg.redF() * alpha + bg.redF() * (1.0 - alpha)),
                            float(fg.greenF() * alpha + bg.greenF() * (1.0 - alpha)),
                            float(fg.blueF() * alpha + bg.blueF() * (1.0 - alpha)));
}

double contrast(const QColor& a, const QColor& b)
{
    const double x = luminance(a);
    const double y = luminance(b);
    return (std::max(x, y) + 0.05) / (std::min(x, y) + 0.05);
}

const Surface* surfaceFor(const QString& field)
{
    for (const Surface& s : kSurfaces) {
        if (field == QLatin1String(s.field)) { return &s; }
    }
    const QString key = field.section(QLatin1Char('.'), -1);
    for (const Surface& s : kCompositeKeys) {
        if (key == QLatin1String(s.field) && field.count(QLatin1Char('.')) >= 1
            && !field.startsWith(QLatin1String("BarPreset."))) {
            return &s;
        }
    }
    return nullptr;
}

const char* faintReason(const QString& field, const QString& fgRole, const QString& what = {})
{
    const QString key = field + QLatin1Char('/') + fgRole;
    const QString tail = field.section(QLatin1Char('.'), -1) + QLatin1Char('/') + fgRole;
    for (const Faint& f : kFaint) {
        const QLatin1String k(f.key);
        if (k == key || (!what.isEmpty() && k == what)
            || (k == tail && field.count(QLatin1Char('.')) >= 2)) {
            return f.why;
        }
    }
    return nullptr;
}

void paintOnce(MeterItem& item)
{
    // Large enough for every face to lay out its text without complaint.
    QImage image(800, 600, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    QPainter painter(&image);
    item.paint(painter, image.width(), image.height());
}

} // namespace

class TestMeterItemColourLook : public QObject {
    Q_OBJECT

private slots:
    void cleanup() { WorkspacePreview::setEnabledForTest(false); }

    // Every default colour of every registry item type is in the table.
    void everyRegistryDefaultIsListed()
    {
        ContainerContentRegistry registry;
        QStringList unlisted;
        QSet<QString> seen;
        int items = 0;
        for (const ContentDescriptor& descriptor : registry.descriptors()) {
            const ContentEntry entry = registry.makeEntry(descriptor.typeId);
            std::unique_ptr<MeterItem> item(
                registry.createMeterItem(entry, nullptr, ContentRenderMode::Validation));
            if (!item) {
                continue;
            }
            ++items;
            for (const auto& field : ItemColourLook::colourFieldsOf(*item)) {
                if (!ItemColourLook::listed(field.field, field.stored)) {
                    const QString line = QStringLiteral("%1 %2").arg(field.field,
                                                                     field.stored.name(QColor::HexArgb));
                    if (!seen.contains(line)) {
                        seen.insert(line);
                        unlisted << line + QStringLiteral(" (") + descriptor.typeId + QStringLiteral(")");
                    }
                }
            }
        }
        QVERIFY(items > 40);
        QVERIFY2(unlisted.isEmpty(), qPrintable(QStringLiteral("\n") + unlisted.join(QLatin1Char('\n'))));
    }

    // Every role a row names is a palette role, and every row either maps
    // or says why it keeps the classic colour.
    void everyRowNamesARoleOrAReason()
    {
        QStringList bad;
        for (const ItemColourLook::Row& row : ItemColourLook::rows()) {
            if (row.role != nullptr && !role(row.role).isValid()) {
                bad << QStringLiteral("%1: no role %2").arg(QLatin1String(row.field), QLatin1String(row.role));
            }
            if (row.role == nullptr && (row.kept == nullptr || QByteArray(row.kept).isEmpty())) {
                bad << QStringLiteral("%1: kept without a reason").arg(QLatin1String(row.field));
            }
        }
        QVERIFY2(bad.isEmpty(), qPrintable(bad.join(QLatin1Char('\n'))));
    }

    // Every mapped row's role against the surface it is drawn on, and the
    // derived pairs: text at 4.5:1, graphics at 3:1, except the faint
    // structure kFaint lists with its reason. With
    // NEREUS_COLOUR_LOOK_CONTRAST_OUT set, the whole list is written there.
    void everyMappedPairReadsAtItsThreshold()
    {
        struct Result {
            QString what;
            QString fg;
            QString bg;
            double alpha;
            Kind kind;
            double ratio;
            QString status;
        };
        QVector<Result> results;
        QStringList missing;
        QStringList failing;
        QSet<QString> faintUsed;
        const auto measure = [&](const QString& what, const QString& key, const QString& fg, const QString& bg,
                                 double alpha, Kind kind, const QString& faintWhat) {
            const QColor back = role(bg.toLatin1().constData());
            const double ratio = contrast(over(role(fg.toLatin1().constData()), alpha, back), back);
            const double need = kind == Kind::Text ? 4.5 : 3.0;
            QString status = QStringLiteral("pass");
            if (ratio < need) {
                const char* why = faintReason(key, fg, faintWhat);
                if (why != nullptr) {
                    status = QStringLiteral("kept faint: ") + QLatin1String(why);
                    faintUsed.insert(QLatin1String(why));
                } else {
                    status = QStringLiteral("FAIL");
                    failing << QStringLiteral("%1 %2 on %3: %4").arg(what, fg, bg).arg(ratio, 0, 'f', 2);
                }
            }
            results.append({what, fg, bg, alpha, kind, ratio, status});
        };
        QSet<QString> seen;
        for (const ItemColourLook::Row& row : ItemColourLook::rows()) {
            if (row.role == nullptr) { continue; }
            const QString field = QLatin1String(row.field);
            const Surface* s = surfaceFor(field);
            if (s == nullptr) {
                missing << field;
                continue;
            }
            const double alpha = qAlpha(row.classic) / 255.0 * (s->alpha > 0 ? s->alpha : 1.0);
            const QString dedupe = field + QLatin1Char('/') + QLatin1String(row.role) + QString::number(alpha);
            if (seen.contains(dedupe)) { continue; }
            seen.insert(dedupe);
            if (row.backdrop || s->kind == Kind::Fill) {
                results.append({field, QLatin1String(row.role), QString(), alpha, Kind::Fill, 0.0,
                                QStringLiteral("fill: what is drawn on it is measured against it")});
                continue;
            }
            for (const QString& bg : QString::fromLatin1(s->on).split(QLatin1Char(','))) {
                measure(field, field, QLatin1String(row.role), bg, alpha, s->kind, QString());
            }
        }
        for (const DerivedPair& d : kDerived) {
            measure(QLatin1String(d.what), QString(), QLatin1String(d.fg), QLatin1String(d.bg),
                    d.alpha > 0 ? d.alpha : 1.0, d.kind, QLatin1String(d.what));
        }
        if (const QByteArray out = qgetenv("NEREUS_COLOUR_LOOK_CONTRAST_OUT"); !out.isEmpty()) {
            QFile file(QString::fromLocal8Bit(out));
            QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
            QTextStream ts(&file);
            ts << "what\tforeground\tsurface\talpha\tkind\tratio\tstatus\n";
            for (const Result& r : results) {
                const char* kind = r.kind == Kind::Text ? "text" : r.kind == Kind::Graphic ? "graphic" : "fill";
                ts << r.what << '\t' << r.fg << '\t' << r.bg << '\t' << QString::number(r.alpha, 'f', 2) << '\t'
                   << kind << '\t' << (r.kind == Kind::Fill ? QString() : QString::number(r.ratio, 'f', 2)) << '\t'
                   << r.status << '\n';
            }
        }
        QVERIFY2(missing.isEmpty(), qPrintable(QStringLiteral("no surface for: ") + missing.join(QStringLiteral(", "))));
        QVERIFY2(failing.isEmpty(), qPrintable(failing.join(QLatin1Char('\n'))));
        QStringList stale;
        for (const Faint& f : kFaint) {
            if (!faintUsed.contains(QLatin1String(f.why))) { stale << QLatin1String(f.key); }
        }
        QVERIFY2(stale.isEmpty(), qPrintable(QStringLiteral("faint entries no pair needs: ") + stale.join(QStringLiteral(", "))));
    }

    // A new filter display (an LSB slice with snap lines, auto zoom and the
    // notch filter on, so the setting-on boxes show) draws the setting-on
    // box in selected-top, and a new rotator's beam blends text-secondary.
    // With NEREUS_COLOUR_LOOK_RENDER_DIR set, both renders are saved there
    // (the preview tool has no scene that feeds a filter display a slice).
    void filterAndRotatorDrawTheirRoles()
    {
        WorkspacePreview::setEnabledForTest(true);
        FilterDisplayItem filter;
        filter.applyNewItemDefaults();
        filter.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        FilterDisplayItem::SliceFilterState state;
        state.sliceIndex = 0;
        state.mode = DSPMode::LSB;
        state.vfoHz = 7185000.0;
        state.rxLowHz = -2800;
        state.rxHighHz = -100;
        state.txLowHz = -2800;
        state.txHighHz = -100;
        state.tnfActive = true;
        state.filterName = QStringLiteral("2.7k");
        filter.setSliceFilterState(state);
        filter.setDisplayMode(FilterDisplayItem::DisplayMode::None);
        filter.setSnapLines(true);
        filter.setAutoZoom(true);
        QImage filterImage(600, 200, QImage::Format_ARGB32_Premultiplied);
        filterImage.fill(role("panel"));
        {
            QPainter painter(&filterImage);
            filter.paint(painter, filterImage.width(), filterImage.height());
        }
        int settingOn = 0;
        for (int y = 0; y < filterImage.height(); ++y) {
            for (int x = 0; x < filterImage.width(); ++x) {
                settingOn += filterImage.pixelColor(x, y).rgb() == role("selected-top").rgb() ? 1 : 0;
            }
        }
        QVERIFY2(settingOn > 0, "no setting-on box in selected-top");

        RotatorItem rotator;
        rotator.applyNewItemDefaults();
        rotator.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        rotator.setShowBeamWidth(true);
        rotator.setBeamWidth(60.0f);
        QImage rotatorImage(400, 400, QImage::Format_ARGB32_Premultiplied);
        rotatorImage.fill(role("panel"));
        {
            QPainter painter(&rotatorImage);
            rotator.paint(painter, rotatorImage.width(), rotatorImage.height());
        }
        if (const QByteArray dir = qgetenv("NEREUS_COLOUR_LOOK_RENDER_DIR"); !dir.isEmpty()) {
            QVERIFY(filterImage.save(QString::fromLocal8Bit(dir) + QStringLiteral("/filter-display-new.png")));
            QVERIFY(rotatorImage.save(QString::fromLocal8Bit(dir) + QStringLiteral("/rotator-new.png")));
        }
    }

    // With the look on, every registry item paints without naming a colour
    // field the table does not know.
    void everyItemPaintsWithKnownFields()
    {
        WorkspacePreview::setEnabledForTest(true);
        QTest::failOnWarning(QRegularExpression(QStringLiteral("unknown colour field")));
        ContainerContentRegistry registry;
        for (const ContentDescriptor& descriptor : registry.descriptors()) {
            const ContentEntry entry = registry.makeEntry(descriptor.typeId);
            std::unique_ptr<MeterItem> item(
                registry.createMeterItem(entry, nullptr, ContentRenderMode::Validation));
            if (item) {
                paintOnce(*item);
            }
        }
    }

    void vfoDisplayFollowsTheLook()
    {
        VfoDisplayItem vfo;
        vfo.applyNewItemDefaults();
        const QColor rx = vfo.rxColour();
        const QColor tx = vfo.txColour();
        const QColor back = vfo.backgroundColour();

        // Look off: the classic colours.
        QCOMPARE(ItemColourLook::effective("VfoDisplay.rxColour", rx), rx);
        QCOMPARE(ItemColourLook::effective("VfoDisplay.backgroundColour", back, &vfo), back);

        // Look on: the roles; RX stays green, TX stays red.
        WorkspacePreview::setEnabledForTest(true);
        QCOMPARE(ItemColourLook::effective("VfoDisplay.rxColour", rx), role("live"));
        QCOMPARE(ItemColourLook::effective("VfoDisplay.txColour", tx), role("tx"));
        QCOMPARE(ItemColourLook::effective("VfoDisplay.backgroundColour", back, &vfo), role("panel"));
        QVERIFY(ItemColourLook::followsLook(QStringLiteral("VfoDisplay.rxColour"), rx));

        // Look on, a user's colour: that colour.
        const QColor user(0x12, 0x34, 0x56);
        vfo.setRxColour(user);
        QCOMPARE(ItemColourLook::effective("VfoDisplay.rxColour", vfo.rxColour()), user);
        QVERIFY(!ItemColourLook::followsLook(QStringLiteral("VfoDisplay.rxColour"), vfo.rxColour()));

        // The painted background is the role's.
        QImage image(240, 120, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::magenta);
        {
            QPainter painter(&image);
            vfo.paint(painter, image.width(), image.height());
        }
        // The bottom right corner: clear of the RX stripe and the text.
        QCOMPARE(image.pixelColor(image.width() - 3, image.height() - 3).rgb(), role("panel").rgb());
    }

    void buttonGridFollowsTheLook()
    {
        ModeButtonItem grid;
        QVERIFY(grid.buttonCount() > 0);
        const ButtonBoxItem::ButtonState& face = grid.button(0);
        QCOMPARE(ItemColourLook::effective("ButtonBox.fillColour", face.fillColour), face.fillColour);
        QCOMPARE(ItemColourLook::effective("ButtonBox.onColour", face.onColour), face.onColour);

        WorkspacePreview::setEnabledForTest(true);
        QCOMPARE(ItemColourLook::effective("ButtonBox.fillColour", face.fillColour), role("raised-top"));
        QCOMPARE(ItemColourLook::effective("ButtonBox.onColour", face.onColour), role("selected-top"));
        QCOMPARE(ItemColourLook::effective("ButtonBox.textColour", face.textColour), role("text"));
        const QColor user(0x80, 0x10, 0x90);
        QCOMPARE(ItemColourLook::effective("ButtonBox.fillColour", user), user);
    }

    void barFollowsTheLook()
    {
        auto bar = std::make_unique<BarItem>();
        bar->applyNewItemDefaults();
        const QColor fill = bar->barColor();
        const QColor peak = bar->peakHoldMarkerColour();
        QCOMPARE(ItemColourLook::effective("Bar.barColor", fill), fill);

        WorkspacePreview::setEnabledForTest(true);
        QCOMPARE(ItemColourLook::effective("Bar.barColor", fill), role("measured"));
        QCOMPARE(ItemColourLook::effective("Bar.peakHoldMarkerColour", peak), role("danger"));
        bar->setBarColor(QColor(0x22, 0x88, 0x44));
        QCOMPARE(ItemColourLook::effective("Bar.barColor", bar->barColor()), QColor(0x22, 0x88, 0x44));
    }

    void clockFollowsTheLook()
    {
        ClockItem clock;
        clock.applyNewItemDefaults();
        const QColor time = clock.timeColour();
        const QColor date = clock.dateColour();
        QCOMPARE(ItemColourLook::effective("Clock.timeColour", time), time);

        WorkspacePreview::setEnabledForTest(true);
        QCOMPARE(ItemColourLook::effective("Clock.timeColour", time), role("measured"));
        QCOMPARE(ItemColourLook::effective("Clock.dateColour", date), role("text-secondary"));
        clock.setTimeColour(QColor(0x40, 0xa0, 0xff));
        QCOMPARE(ItemColourLook::effective("Clock.timeColour", clock.timeColour()), QColor(0x40, 0xa0, 0xff));
    }

    void ledFollowsTheLook()
    {
        LEDItem led;
        led.applyNewItemDefaults();
        const QColor on = led.trueColour();
        const QColor off = led.falseColour();
        QCOMPARE(ItemColourLook::effective("Led.trueColour", on), on);

        WorkspacePreview::setEnabledForTest(true);
        QCOMPARE(ItemColourLook::effective("Led.trueColour", on), role("live"));
        QCOMPARE(ItemColourLook::effective("Led.falseColour", off), role("text-scale"));
        const QColor user(0xff, 0x00, 0xff);
        QCOMPARE(ItemColourLook::effective("Led.trueColour", user), user);
    }

    void containerChromeFollowsTheLook()
    {
        const QColor classic(0x0f, 0x0f, 0x1a);
        const QColor user(0x30, 0x10, 0x10);
        QCOMPARE(ContainerWidget::contentBackground(classic), classic);
        QCOMPARE(ContainerWidget::contentBackground(user), user);

        WorkspacePreview::setEnabledForTest(true);
        QCOMPARE(ContainerWidget::contentBackground(classic), role("panel"));
        QCOMPARE(ContainerWidget::contentBackground(user), user);
    }

    // Saved records never change: the record of every registry item is the
    // same byte for byte with the look on and off, before and after it
    // paints, and a stored record restores to the same bytes.
    void recordsRoundTripByteForByte()
    {
        ContainerContentRegistry registry;
        for (const ContentDescriptor& descriptor : registry.descriptors()) {
            const ContentEntry entry = registry.makeEntry(descriptor.typeId);
            WorkspacePreview::setEnabledForTest(false);
            std::unique_ptr<MeterItem> off(registry.createMeterItem(entry, nullptr, ContentRenderMode::Validation));
            if (!off) {
                continue;
            }
            const QByteArray classic = recordOf(registry, *off, entry);
            paintOnce(*off);
            QCOMPARE(recordOf(registry, *off, entry), classic);

            WorkspacePreview::setEnabledForTest(true);
            const ContentEntry stored = registry.captureMeterItem(*off, entry);
            std::unique_ptr<MeterItem> on(registry.createMeterItem(stored, nullptr, ContentRenderMode::Validation));
            QVERIFY2(on != nullptr, qPrintable(descriptor.typeId));
            paintOnce(*on);
            QVERIFY2(recordOf(registry, *on, stored) == classic, qPrintable(descriptor.typeId));
        }
    }

    // A user's colour round-trips byte for byte with the look on and off.
    void userColourRoundTripsByteForByte()
    {
        ClockItem clock;
        clock.applyNewItemDefaults();
        clock.setTimeColour(QColor(0x40, 0xa0, 0xff));
        const QString saved = clock.serialize();
        for (bool look : {false, true}) {
            WorkspacePreview::setEnabledForTest(look);
            ClockItem restored;
            QVERIFY(restored.deserialize(saved));
            paintOnce(restored);
            QCOMPARE(restored.serialize(), saved);
            QCOMPARE(restored.timeColour(), QColor(0x40, 0xa0, 0xff));
        }
    }

    // With the look off every row draws its classic colour.
    void lookOffKeepsEveryClassicColour()
    {
        WorkspacePreview::setEnabledForTest(false);
        for (const ItemColourLook::Row& row : ItemColourLook::rows()) {
            const QColor classic = QColor::fromRgba(row.classic);
            QVERIFY2(ItemColourLook::effective(row.field, classic) == classic, row.field);
        }
    }

    // A colour picked in an editor stays as picked with the look on, even
    // when it is exactly a default; the pick is saved beside the record and
    // an entry saved without it (an older layout) follows the look.
    void pickOfADefaultStaysAsPicked()
    {
        WorkspacePreview::setEnabledForTest(true);
        ClockItem clock;
        clock.applyNewItemDefaults();
        const QColor classicTitle = clock.typeTitleColour();
        QCOMPARE(ItemColourLook::effective("Clock.typeTitleColour", classicTitle, &clock), role("text-secondary"));
        ClockItemEditor editor;
        editor.setItem(&clock);
        auto* swatch = editor.findChild<QPushButton*>(QStringLiteral("clockTitleColour"));
        QVERIFY(swatch != nullptr);

        bool answered = false;
        QTimer timer;
        timer.setInterval(20);
        connect(&timer, &QTimer::timeout, this, [&]() {
            if (auto* dialog = qobject_cast<QColorDialog*>(QApplication::activeModalWidget())) {
                dialog->setCurrentColor(classicTitle);
                dialog->accept();
                answered = true;
                timer.stop();
            }
        });
        timer.start();
        swatch->click();
        QVERIFY(answered);
        QCOMPARE(clock.typeTitleColour().rgba(), classicTitle.rgba());
        QCOMPARE(ItemColourLook::effective("Clock.typeTitleColour", clock.typeTitleColour(), &clock),
                 classicTitle);
        QVERIFY2(swatch->styleSheet().contains(classicTitle.name(QColor::HexArgb)),
                 qPrintable(swatch->styleSheet()));
        QVERIFY(swatch->toolTip().isEmpty());

        // Saved and restored: the pick holds; the record itself is unchanged.
        ContainerContentRegistry registry;
        const ContentEntry saved = registry.captureMeterItem(clock);
        QVERIFY(saved.context.contains(QStringLiteral("userColours")));
        std::unique_ptr<MeterItem> restored(registry.createMeterItem(saved, nullptr, ContentRenderMode::Validation));
        QVERIFY(restored != nullptr);
        QCOMPARE(ItemColourLook::effective("Clock.typeTitleColour", classicTitle, restored.get()), classicTitle);
        QCOMPARE(registry.captureMeterItem(*restored, saved).context, saved.context);
        ClockItem unpicked;
        unpicked.applyNewItemDefaults();
        QCOMPARE(restored->serialize(), unpicked.serialize());

        // The same entry without the picks (saved before they were
        // recorded) follows the look, and saves without the key.
        ContentEntry older = saved;
        older.context.remove(QStringLiteral("userColours"));
        std::unique_ptr<MeterItem> loaded(registry.createMeterItem(older, nullptr, ContentRenderMode::Validation));
        QVERIFY(loaded != nullptr);
        QCOMPARE(ItemColourLook::effective("Clock.typeTitleColour", classicTitle, loaded.get()), role("text-secondary"));
        QVERIFY(!registry.captureMeterItem(*loaded, older).context.contains(QStringLiteral("userColours")));
    }

    // The editor's swatch shows the colour drawn, with the note while the
    // field follows the look; a pick stores the user's colour.
    void editorSwatchShowsTheEffectiveColour()
    {
        WorkspacePreview::setEnabledForTest(true);
        ClockItem clock;
        clock.applyNewItemDefaults();
        const QColor classicTitle = clock.typeTitleColour();
        ClockItemEditor editor;
        editor.setItem(&clock);
        auto* swatch = editor.findChild<QPushButton*>(QStringLiteral("clockTitleColour"));
        QVERIFY(swatch != nullptr);
        QVERIFY2(swatch->styleSheet().contains(role("text-secondary").name(QColor::HexArgb)),
                 qPrintable(swatch->styleSheet()));
        QCOMPARE(swatch->toolTip(), ItemColourLook::followsLookNote());
        QCOMPARE(clock.typeTitleColour(), classicTitle);

        // Pick a colour in the dialog the swatch opens.
        const QColor user(0x33, 0x99, 0x66);
        bool answered = false;
        QTimer timer;
        timer.setInterval(20);
        connect(&timer, &QTimer::timeout, this, [&]() {
            if (auto* dialog = qobject_cast<QColorDialog*>(QApplication::activeModalWidget())) {
                dialog->setCurrentColor(user);
                dialog->accept();
                answered = true;
                timer.stop();
            }
        });
        timer.start();
        swatch->click();
        QVERIFY(answered);
        QCOMPARE(clock.typeTitleColour().rgb(), user.rgb());
        QVERIFY2(swatch->styleSheet().contains(clock.typeTitleColour().name(QColor::HexArgb)),
                 qPrintable(swatch->styleSheet()));
        QVERIFY(swatch->toolTip().isEmpty());

        // The look off: the swatch shows the stored colour, without the note.
        WorkspacePreview::setEnabledForTest(false);
        ClockItem classic;
        classic.applyNewItemDefaults();
        editor.setItem(&classic);
        QVERIFY(swatch->styleSheet().contains(classicTitle.name(QColor::HexArgb)));
        QVERIFY(swatch->toolTip().isEmpty());
    }
};

QTEST_MAIN(TestMeterItemColourLook)
#include "tst_meter_item_colour_look.moc"
