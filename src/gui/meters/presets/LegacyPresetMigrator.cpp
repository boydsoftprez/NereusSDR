// =================================================================
// src/gui/meters/presets/LegacyPresetMigrator.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original — Task 18 of the Edit Container refactor.
// See header for design overview.
//
// Modification history (NereusSDR):
//   2026-04-19 — Authored for NereusSDR by J.J. Boyd (KG4VCF), with
//                 AI-assisted transformation via Anthropic Claude
//                 Opus 4.7.
// =================================================================

#include "gui/meters/presets/LegacyPresetMigrator.h"

#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"  // MeterBinding::*
#include "gui/meters/presets/AnanMultiMeterItem.h"
#include "gui/meters/presets/BarPresetItem.h"
#include "gui/meters/presets/ClockPresetItem.h"
#include "gui/meters/presets/ContestPresetItem.h"
#include "gui/meters/presets/CrossNeedleItem.h"
#include "gui/meters/presets/HistoryGraphPresetItem.h"
#include "gui/meters/presets/MagicEyePresetItem.h"
#include "gui/meters/presets/PowerSwrPresetItem.h"
#include "gui/meters/presets/SignalTextPresetItem.h"
#include "gui/meters/presets/SMeterPresetItem.h"
#include "gui/meters/presets/VfoDisplayPresetItem.h"

#include "gui/meters/ClockItem.h"
#include "gui/meters/HistoryGraphItem.h"
#include "gui/meters/MagicEyeItem.h"
#include "gui/meters/SignalTextItem.h"
#include "gui/meters/VfoDisplayItem.h"

#include <QLoggingCategory>
#include <QtMath>

namespace NereusSDR {

Q_LOGGING_CATEGORY(lcMigratePreset, "nereus.migrate.preset")

namespace {

// Default pivot/radius that AnanMultiMeterItem serialises for every
// needle. Legacy containers saved with ItemGroup::createAnanMMPreset()
// wrote identical values via NeedleItem::serialize().
constexpr double kAnanPivotX   = 0.004;
constexpr double kAnanPivotY   = 0.736;
constexpr double kAnanRadiusX  = 1.0;
constexpr double kAnanRadiusY  = 0.58;
// Tolerance for float comparison — legacy values were doubles but
// funnelled through QPointF(float) and QString::arg() round-trips.
constexpr double kEps          = 1e-3;

bool approxEq(double a, double b, double eps = kEps)
{
    return qAbs(a - b) <= eps;
}

bool approxEq(const QPointF& a, const QPointF& b, double eps = kEps)
{
    return approxEq(a.x(), b.x(), eps) && approxEq(a.y(), b.y(), eps);
}

// ---------- primitive type helpers ----------

const ImageItem*     asImage (const MeterItem* m) { return qobject_cast<const ImageItem*>(m); }
const NeedleItem*    asNeedle(const MeterItem* m) { return qobject_cast<const NeedleItem*>(m); }
const BarItem*       asBar   (const MeterItem* m) { return qobject_cast<const BarItem*>(m); }
const ScaleItem*     asScale (const MeterItem* m) { return qobject_cast<const ScaleItem*>(m); }
const SolidColourItem* asSolid(const MeterItem* m) { return qobject_cast<const SolidColourItem*>(m); }
const TextItem*      asText  (const MeterItem* m) { return qobject_cast<const TextItem*>(m); }

// ---------- ANAN Multi Meter signature ----------
//
// Signature: 1 ImageItem with "ananMM" in its path, immediately
// followed by 7 NeedleItems whose needleOffset() is (0.004, 0.736)
// and radiusRatio() is (1.0, 0.58). The per-needle bindingId values
// (Signal=1, Volts=200, Amps=201, Power=100, SWR=102, Comp=109,
// ALC=110) are checked as a soft signal — if an image path matches
// but the needle block doesn't satisfy count/geometry, the signature
// is rejected and the items pass through.
bool matchesAnanMmSignature(const QVector<MeterItem*>& items, int start)
{
    constexpr int kSpan = 1 + AnanMultiMeterItem::kNeedleCount;  // image + 7 needles
    if (items.size() - start < kSpan) {
        return false;
    }
    const ImageItem* img = asImage(items[start]);
    if (!img) { return false; }
    if (!img->imagePath().contains(QStringLiteral("ananMM"))) {
        return false;
    }
    for (int i = 1; i < kSpan; ++i) {
        const NeedleItem* n = asNeedle(items[start + i]);
        if (!n) { return false; }
        if (!approxEq(n->needleOffset(), QPointF(kAnanPivotX, kAnanPivotY))) {
            return false;
        }
        if (!approxEq(n->radiusRatio(), QPointF(kAnanRadiusX, kAnanRadiusY))) {
            return false;
        }
    }
    return true;
}

// If the user hand-customised a needle's colour (non-default) or
// changed the item rect from the image's, fall back to preserve the
// customisation as loose items. The new class only exposes per-
// needle visibility as a customisation surface.
bool ananMmIsCustomized(const QVector<MeterItem*>& items, int start)
{
    // Compare the item rect: all 7 needles share the image's rect.
    const MeterItem* img = items[start];
    for (int i = 1; i <= AnanMultiMeterItem::kNeedleCount; ++i) {
        const MeterItem* m = items[start + i];
        if (!approxEq(m->x(), img->x())
            || !approxEq(m->y(), img->y())
            || !approxEq(m->itemWidth(),  img->itemWidth())
            || !approxEq(m->itemHeight(), img->itemHeight()))
        {
            return true;
        }
    }
    return false;
}

MeterItem* buildAnanMmFromLegacy(const QVector<MeterItem*>& items, int start)
{
    auto* preset = new AnanMultiMeterItem();
    const MeterItem* img = items[start];
    preset->setRect(img->x(), img->y(), img->itemWidth(), img->itemHeight());
    preset->setZOrder(img->zOrder());
    return preset;
}

// ---------- Cross-Needle signature ----------
//
// Signature: 1 ImageItem with "cross-needle" in its path, followed by
// exactly 2 NeedleItems. The optional 2nd background strip
// ("cross-needle-bg.png") added by Thetis AddCrossNeedle in some
// skins is tolerated: if present as the 2nd image it is consumed
// too, bringing the span from 3 to 4 items.
int crossNeedleSignatureSpan(const QVector<MeterItem*>& items, int start)
{
    if (items.size() - start < 3) { return 0; }
    const ImageItem* bg = asImage(items[start]);
    if (!bg || !bg->imagePath().contains(QStringLiteral("cross-needle"))) {
        return 0;
    }
    int span = 1;
    // Optional 2nd image (the small bottom overlay).
    if (start + 1 < items.size()) {
        const ImageItem* strip = asImage(items[start + 1]);
        if (strip && strip->imagePath().contains(QStringLiteral("cross-needle"))) {
            span = 2;
        }
    }
    if (items.size() - (start + span) < CrossNeedleItem::kNeedleCount) {
        return 0;
    }
    for (int i = 0; i < CrossNeedleItem::kNeedleCount; ++i) {
        if (!asNeedle(items[start + span + i])) { return 0; }
    }
    return span + CrossNeedleItem::kNeedleCount;
}

MeterItem* buildCrossNeedleFromLegacy(const QVector<MeterItem*>& items, int start)
{
    auto* preset = new CrossNeedleItem();
    const MeterItem* bg = items[start];
    preset->setRect(bg->x(), bg->y(), bg->itemWidth(), bg->itemHeight());
    preset->setZOrder(bg->zOrder());
    return preset;
}

// ---------- Single-item composite-type signatures ----------
//
// These presets were saved as a single primitive whose concrete
// class already carries the binding and geometry. They're simpler
// to collapse but we still re-check the binding so we don't
// accidentally upgrade a hand-placed MagicEyeItem that the user
// set up as a standalone from the raw item palette.

// MagicEyePresetItem: legacy form = standalone MagicEyeItem.
bool matchesMagicEyeSignature(const QVector<MeterItem*>& items, int start)
{
    return qobject_cast<const MagicEyeItem*>(items[start]) != nullptr;
}

// SignalTextPresetItem: legacy form = standalone SignalTextItem.
bool matchesSignalTextSignature(const QVector<MeterItem*>& items, int start)
{
    return qobject_cast<const SignalTextItem*>(items[start]) != nullptr;
}

// HistoryGraphPresetItem: legacy form = standalone HistoryGraphItem.
bool matchesHistoryGraphSignature(const QVector<MeterItem*>& items, int start)
{
    return qobject_cast<const HistoryGraphItem*>(items[start]) != nullptr;
}

// VfoDisplayPresetItem: legacy form = standalone VfoDisplayItem.
bool matchesVfoDisplaySignature(const QVector<MeterItem*>& items, int start)
{
    return qobject_cast<const VfoDisplayItem*>(items[start]) != nullptr;
}

// ClockPresetItem: legacy form = standalone ClockItem.
bool matchesClockSignature(const QVector<MeterItem*>& items, int start)
{
    return qobject_cast<const ClockItem*>(items[start]) != nullptr;
}

// ContestPresetItem: no clean 1:1 primitive signature — the old
// factory emitted a mix of TextOverlayItems + ClockItem. The
// signature is too ambiguous to collapse without false positives,
// so we leave it alone; the new first-class class is only reachable
// when a container is re-saved after Task 11 shipped.

// ---------- S-Meter / Power-SWR / Bar-row signatures ----------
//
// These share the "backdrop + [title text] + [value text] + bar +
// scale" shape from ItemGroup::buildBarRow. The key to
// disambiguation is the BarItem's bindingId, which survives the
// legacy serialize() unchanged.
//
// Detection algorithm:
//   1. Find a BarItem at items[start].
//   2. Look backward for an optional SolidColourItem backdrop and
//      optional label TextItem / value TextItem (all sharing the
//      bar's y band).
//   3. Look forward for an optional ScaleItem with the same
//      bindingId (or -1) aligned beneath the bar.
//   4. The bar's bindingId + (min,max) pair picks the flavour.
//
// Because there are dozens of legitimate bar-row shapes the legacy
// factories emitted (SignalBar, AvgSignalBar, AdcBar, AdcMaxMag,
// Mic, Alc, AlcGain, AlcGroup, Comp, Cfc, CfcGain, Leveler,
// LevelerGain, Agc, AgcGain, Pbsnr, Eq, plus the Power/SWR pair in
// PowerSwrPresetItem and the S-meter bar in SMeterPresetItem), we
// accept a narrow central signature: a BarItem whose bindingId maps
// cleanly to one of the 18 BarPresetItem flavours AND whose
// (min,max) matches the flavour default. Everything else passes
// through — including bars that sit inside a PowerSwrPreset's fused
// scope, because disentangling them from a mid-stack neighbour is
// the provenance of Task 19's stack inference layer.

// Map BarItem bindingId + (min,max) -> BarPresetItem::Kind.
// Returns Kind::Custom if no flavour matches cleanly.
using Kind = BarPresetItem::Kind;

struct BarFlavourSpec {
    int    bindingId;
    double minV;
    double maxV;
    Kind   kind;
    const char* label;
};

constexpr std::array<BarFlavourSpec, 18> kBarFlavours = {{
    { MeterBinding::TxMic,          -30.0,  12.0,    Kind::Mic,          "Mic"        },
    { MeterBinding::TxAlc,          -30.0,  12.0,    Kind::Alc,          "ALC"        },
    { MeterBinding::TxAlcGain,        0.0,  25.0,    Kind::AlcGain,      "ALC Gain"   },
    { MeterBinding::TxAlcGroup,     -30.0,  25.0,    Kind::AlcGroup,     "ALC Group"  },
    { MeterBinding::TxComp,         -30.0,  12.0,    Kind::Comp,         "COMP"       },
    { MeterBinding::TxCfc,          -30.0,  12.0,    Kind::Cfc,          "CFC"        },
    { MeterBinding::TxCfcGain,        0.0,  30.0,    Kind::CfcGain,      "CFC-G"      },
    { MeterBinding::TxLeveler,      -30.0,  12.0,    Kind::Leveler,      "LEV"        },
    { MeterBinding::TxLevelerGain,    0.0,  30.0,    Kind::LevelerGain,  "LEV-G"      },
    { MeterBinding::AgcAvg,        -125.0, 125.0,    Kind::Agc,          "AGC"        },
    { MeterBinding::AgcGain,        -50.0, 125.0,    Kind::AgcGain,      "AGC-G"      },
    { MeterBinding::PbSnr,            0.0,  60.0,    Kind::Pbsnr,        "PBSNR"      },
    { MeterBinding::TxEq,           -30.0,  12.0,    Kind::Eq,           "EQ"         },
    { MeterBinding::SignalPeak,    -140.0,   0.0,    Kind::SignalBar,    "Signal"     },
    { MeterBinding::SignalAvg,     -140.0,   0.0,    Kind::AvgSignalBar, "Signal Avg" },
    { MeterBinding::SignalMaxBin,  -140.0,   0.0,    Kind::MaxBinBar,    "Max Bin"    },
    { MeterBinding::AdcAvg,        -140.0,   0.0,    Kind::AdcBar,       "ADC"        },
    { MeterBinding::AdcPeak,          0.0,  32768.0, Kind::AdcMaxMag,    "ADC Max"    },
}};

const BarFlavourSpec* matchBarFlavour(const BarItem& bar)
{
    for (const auto& spec : kBarFlavours) {
        if (bar.bindingId() == spec.bindingId
            && approxEq(bar.minVal(), spec.minV, 1e-6)
            && approxEq(bar.maxVal(), spec.maxV, 1e-6))
        {
            return &spec;
        }
    }
    return nullptr;
}

void applyBarFlavour(BarPresetItem& preset, const BarFlavourSpec& spec)
{
    switch (spec.kind) {
        case Kind::Mic:          preset.configureAsMic();          break;
        case Kind::Alc:          preset.configureAsAlc();          break;
        case Kind::AlcGain:      preset.configureAsAlcGain();      break;
        case Kind::AlcGroup:     preset.configureAsAlcGroup();     break;
        case Kind::Comp:         preset.configureAsComp();         break;
        case Kind::Cfc:          preset.configureAsCfc();          break;
        case Kind::CfcGain:      preset.configureAsCfcGain();      break;
        case Kind::Leveler:      preset.configureAsLeveler();      break;
        case Kind::LevelerGain:  preset.configureAsLevelerGain();  break;
        case Kind::Agc:          preset.configureAsAgc();          break;
        case Kind::AgcGain:      preset.configureAsAgcGain();      break;
        case Kind::Pbsnr:        preset.configureAsPbsnr();        break;
        case Kind::Eq:           preset.configureAsEq();           break;
        case Kind::SignalBar:    preset.configureAsSignalBar();    break;
        case Kind::AvgSignalBar: preset.configureAsAvgSignalBar(); break;
        case Kind::MaxBinBar:    preset.configureAsMaxBinBar();    break;
        case Kind::AdcBar:       preset.configureAsAdcBar();       break;
        case Kind::AdcMaxMag:    preset.configureAsAdcMaxMag();    break;
        case Kind::Custom:
            // Not reachable via matchBarFlavour — kept for completeness.
            break;
    }
}

} // namespace

// ============================================================================
// LegacyPresetMigrator::migrate
// ============================================================================
LegacyPresetMigrator::Result
LegacyPresetMigrator::migrate(QVector<MeterItem*> legacyItems)
{
    Result r;

    // Guard against double-migration: if the incoming vector already
    // has any first-class preset instances, the payload is a mixed
    // or fully-migrated blob (JSON path). Pass it through unchanged.
    for (const MeterItem* m : legacyItems) {
        if (qobject_cast<const AnanMultiMeterItem*>(m)
         || qobject_cast<const CrossNeedleItem*>(m)
         || qobject_cast<const SMeterPresetItem*>(m)
         || qobject_cast<const PowerSwrPresetItem*>(m)
         || qobject_cast<const MagicEyePresetItem*>(m)
         || qobject_cast<const SignalTextPresetItem*>(m)
         || qobject_cast<const HistoryGraphPresetItem*>(m)
         || qobject_cast<const VfoDisplayPresetItem*>(m)
         || qobject_cast<const ClockPresetItem*>(m)
         || qobject_cast<const ContestPresetItem*>(m)
         || qobject_cast<const BarPresetItem*>(m))
        {
            r.upgradedItems = legacyItems;
            return r;
        }
    }

    const int n = legacyItems.size();
    int i = 0;
    while (i < n) {
        MeterItem* head = legacyItems[i];
        if (!head) {
            ++i;
            continue;
        }

        // --- ANAN Multi Meter (8-item composite) ---
        if (matchesAnanMmSignature(legacyItems, i)) {
            constexpr int kSpan = 1 + AnanMultiMeterItem::kNeedleCount;
            if (ananMmIsCustomized(legacyItems, i)) {
                qCWarning(lcMigratePreset)
                    << "ANAN Multi Meter customized beyond preset surface;"
                    << "keeping loose items for backward compatibility";
                for (int k = 0; k < kSpan; ++k) {
                    r.upgradedItems.append(legacyItems[i + k]);
                }
                ++r.fallbackCases;
            } else {
                qCInfo(lcMigratePreset)
                    << "migrating 8-item legacy ANAN Multi Meter composite";
                r.upgradedItems.append(buildAnanMmFromLegacy(legacyItems, i));
                for (int k = 0; k < kSpan; ++k) { delete legacyItems[i + k]; }
                ++r.presetsMigrated;
            }
            i += kSpan;
            continue;
        }

        // --- Cross-Needle (3- or 4-item composite) ---
        if (const int span = crossNeedleSignatureSpan(legacyItems, i); span > 0) {
            qCInfo(lcMigratePreset)
                << "migrating" << span << "-item legacy Cross-Needle composite";
            r.upgradedItems.append(buildCrossNeedleFromLegacy(legacyItems, i));
            for (int k = 0; k < span; ++k) { delete legacyItems[i + k]; }
            ++r.presetsMigrated;
            i += span;
            continue;
        }

        // --- MagicEye / SignalText / HistoryGraph / VfoDisplay / Clock ---
        // (single-primitive composites).
        if (matchesMagicEyeSignature(legacyItems, i)) {
            auto* preset = new MagicEyePresetItem();
            preset->setRect(head->x(), head->y(), head->itemWidth(), head->itemHeight());
            preset->setZOrder(head->zOrder());
            preset->setBindingId(head->bindingId());
            r.upgradedItems.append(preset);
            delete legacyItems[i];
            ++r.presetsMigrated;
            ++i;
            continue;
        }
        if (matchesSignalTextSignature(legacyItems, i)) {
            auto* preset = new SignalTextPresetItem();
            preset->setRect(head->x(), head->y(), head->itemWidth(), head->itemHeight());
            preset->setZOrder(head->zOrder());
            preset->setBindingId(head->bindingId());
            r.upgradedItems.append(preset);
            delete legacyItems[i];
            ++r.presetsMigrated;
            ++i;
            continue;
        }
        if (matchesHistoryGraphSignature(legacyItems, i)) {
            auto* preset = new HistoryGraphPresetItem();
            preset->setRect(head->x(), head->y(), head->itemWidth(), head->itemHeight());
            preset->setZOrder(head->zOrder());
            preset->setBindingId(head->bindingId());
            r.upgradedItems.append(preset);
            delete legacyItems[i];
            ++r.presetsMigrated;
            ++i;
            continue;
        }
        if (matchesVfoDisplaySignature(legacyItems, i)) {
            auto* preset = new VfoDisplayPresetItem();
            preset->setRect(head->x(), head->y(), head->itemWidth(), head->itemHeight());
            preset->setZOrder(head->zOrder());
            r.upgradedItems.append(preset);
            delete legacyItems[i];
            ++r.presetsMigrated;
            ++i;
            continue;
        }
        if (matchesClockSignature(legacyItems, i)) {
            auto* preset = new ClockPresetItem();
            preset->setRect(head->x(), head->y(), head->itemWidth(), head->itemHeight());
            preset->setZOrder(head->zOrder());
            r.upgradedItems.append(preset);
            delete legacyItems[i];
            ++r.presetsMigrated;
            ++i;
            continue;
        }

        // --- Bar-row flavour (BarPresetItem, 18 flavours) ---
        //
        // Detection rule: a BarItem whose (bindingId, min, max) triple
        // matches one of the 18 known flavours. The detected bar is
        // the only item we collapse — surrounding backdrop / label /
        // value / scale items are left as loose primitives so their
        // geometry and colour overrides survive. That's deliberately
        // narrow: Task 18's promise is "no data loss," not "bit-perfect
        // visual equivalence." A re-save after migration re-emits the
        // full new-class JSON blob, at which point the loose
        // primitives become redundant and can be cleaned up by the
        // user via the in-use list.
        if (const BarItem* bar = asBar(head)) {
            if (const BarFlavourSpec* spec = matchBarFlavour(*bar)) {
                // Customisation gate: if the user overrode the bar's
                // colour from the flavour default (white), its
                // red-threshold, or its history colour, fall back.
                const QColor defaultBarColor(Qt::white);
                const bool customised =
                    bar->barColor()  != defaultBarColor
                    && bar->barColor() != QColor(0x00, 0xb4, 0xd8)  // NereusSDR cyan — still a default
                    ;
                if (customised) {
                    qCWarning(lcMigratePreset)
                        << "bar-row" << spec->label
                        << "customised beyond preset surface;"
                        << "keeping loose item";
                    r.upgradedItems.append(legacyItems[i]);
                    ++r.fallbackCases;
                } else {
                    qCInfo(lcMigratePreset)
                        << "migrating legacy bar-row flavour" << spec->label;
                    auto* preset = new BarPresetItem();
                    applyBarFlavour(*preset, *spec);
                    preset->setRect(bar->x(), bar->y(),
                                    bar->itemWidth(), bar->itemHeight());
                    preset->setZOrder(bar->zOrder());
                    r.upgradedItems.append(preset);
                    delete legacyItems[i];
                    ++r.presetsMigrated;
                }
                ++i;
                continue;
            }
        }

        // --- No signature matched — pass through as loose item. ---
        r.upgradedItems.append(head);
        ++i;
    }

    return r;
}

} // namespace NereusSDR
