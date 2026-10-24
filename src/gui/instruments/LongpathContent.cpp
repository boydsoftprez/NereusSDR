// src/gui/instruments/LongpathContent.cpp

// --- From src/gui/applets/BandwidthFilterApplet.cpp ---
// =================================================================
// src/gui/applets/BandwidthFilterApplet.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Begruendung steht im Header.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-20 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// =================================================================
// src/gui/instruments/LongpathContent.cpp  (NereusSDR)
// =================================================================
//
// Ported from Longpath source (values only):
//   src/gui/applets/BandwidthFilterApplet.cpp (passband trace refresh
//   interval and sample density) at revision
//   551576e30c42731c6d51ce005c5f23ec38be391c,
//   https://github.com/oe5sos/Longpath, primary author Martin Fischer
//   (OE5SOS). The upstream banner is copied above.
//
//   Upstream source has no top-of-file copyright or GPL header; the
//   project-level LICENSE (GPLv3) applies.
//
//   Everything else is NereusSDR-original: the registry factories, the
//   slice and MeterPoller bindings, and traceFromRendered(), which reads
//   SpectrumWidget::renderedPixels() instead of Longpath's
//   SpectrumWidget::dbmOverRange().
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-12 - Shared accessory display readings and independent legal
//                 marker / display scale by J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
//   2026-10-07 - Written for the workspace visual review by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The frequency instrument's VFO row uses the slice
//                 letters. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-08 - Needles and bars register their context with the
//                 poller (MeterPoller::addReadingConsumer), so they read
//                 live without a Thetis meter face in the same context,
//                 and drop the old slice's value and peak when the slice
//                 behind the context changes.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The frequency instrument refreshes the other slice and the
//                 active marker on every slice change; a no-meter reading on
//                 the secondary binding clears it; the upstream comment on
//                 the trace interval is copied to its constant.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - Container Settings previews build the instruments inert
//                 (ContentRenderMode::Preview): no reading consumer, slice
//                 follower or trace timer, seeded once from the cached
//                 readings and the slice. A needle or bar's registration
//                 replay reaches that widget alone.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The four types declare their editable properties for the
//                 Container Settings property editor (readings, frequency
//                 form, bar compact, filter label); the bar's compact form
//                 and the filter pane's label come from the entry config.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-20 - Frequency (Custom) registers beside Frequency (Longpath)
//                 and applies font size, colour, centering and the slice
//                 lines from the entry config. Brian Sutton,
//                 AI-assisted via xAI Grok.
//   2026-10-23 - The slice follower also rebinds when station ownership or
//                 the selected RX changes. A direct connection publishes the
//                 slice before the station adopts it, and that adoption does
//                 not emit activeSliceChanged when the slice is already
//                 active, so the frequency stayed on the empty reading.
//                 Brian Sutton, AI-assisted via xAI Grok.
// =================================================================

#include "models/MainPowerMeter.h"
#include "gui/instruments/PowerReadingDescriptor.h"
#include "gui/instruments/LongpathContent.h"

#include "core/ControlRanges.h"
#include "core/SliceOwnership.h"
#include "gui/SpectrumWidget.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/instruments/BarInstrument.h"
#include "gui/instruments/FrequencyInstrument.h"
#include "gui/instruments/NeedleInstrument.h"
#include "gui/instruments/ReadingSource.h"
#include "gui/meters/MeterItem.h"
#include "gui/meters/MeterPoller.h"
#include "gui/widgets/BandwidthFilterPane.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QColor>
#include <QPointer>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>

namespace NereusSDR::LongpathContent {

namespace {

// 20 Hz. Schneller waere Verschwendung: die Kurve dient dem
// Augenmass beim Kantenziehen, nicht der Signalsuche — dafuer
// ist der Panadapter da.
// [original inline comment from BandwidthFilterApplet.cpp:779-781]
// From Longpath src/gui/applets/BandwidthFilterApplet.cpp:783 [@551576e3]
constexpr int kTraceIntervalMs = 50;
// From Longpath src/gui/applets/BandwidthFilterApplet.cpp:835-837 [@551576e3]
constexpr int kTraceMinPoints = 64;
constexpr int kTraceMaxPoints = 400;
constexpr double kTraceDevicePxPerPoint = 12.0;

QString sliceLabel(const SliceModel* slice)
{
    if (!slice) { return QStringLiteral("RX"); }
    return QStringLiteral("RX %1").arg(QChar(QLatin1Char('A').unicode() + slice->sliceIndex()));
}

// Rebinds an instrument when the slice its context names may have
// changed (a slice opened or closed, the selected RX moved, or the
// station took the slice).
//
// Two parts: `bind` wires the instrument to its own slice and runs only
// when that slice changes; `refresh` covers what depends on the other
// slices and on which slice is active, and runs on every slice added,
// removed or made active, even when the own slice is unchanged.
//
// Ownership is separate from those three RadioModel signals. On a direct
// connection the slice is published while it is still unowned, and the
// station adopts it from its own sliceAdded slot. applyActiveSlices then
// sees the same active slice and emits nothing. The instrument has to
// ask for the slice again on SliceOwnership::activeChanged, or it stays
// on the empty reading. activeRxChanged covers a receive-focus move that
// does not change which slice is active.
class SliceFollower : public QObject {
public:
    SliceFollower(QWidget* owner, const Sources& sources, QJsonObject context)
        : QObject(owner), m_sources(sources), m_context(std::move(context))
    {
    }

    void start(std::function<void(SliceModel*)> bind,
               std::function<void(SliceModel*)> refresh = {})
    {
        m_bind = std::move(bind);
        m_refresh = std::move(refresh);
        if (RadioModel* radio = m_sources.radio) {
            connect(radio, &RadioModel::sliceAdded, this, [this](int) { rebind(); });
            connect(radio, &RadioModel::sliceRemoved, this, [this](int) { rebind(); });
            connect(radio, &RadioModel::activeSliceChanged, this, [this](int) { rebind(); });
            if (SliceOwnership* ownership = radio->sliceOwnership()) {
                connect(ownership, &SliceOwnership::activeChanged, this, [this] { rebind(); });
                connect(ownership, &SliceOwnership::activeRxChanged, this,
                        [this](const QByteArray&) { rebind(); });
            }
        }
        rebind();
    }

private:
    static void dropAll(QVector<QMetaObject::Connection>& connections)
    {
        for (const auto& c : std::as_const(connections)) { disconnect(c); }
        connections.clear();
    }

    void rebind()
    {
        SliceModel* slice = m_sources.sliceForContext ? m_sources.sliceForContext(m_context) : nullptr;
        if (slice != m_slice || !m_bound) {
            dropAll(m_connections);
            m_slice = slice;
            m_bound = true;
            m_bind(slice);
        }
        dropAll(m_refreshConnections);
        if (m_refresh) { m_refresh(slice); }
    }

public:
    // A connection that belongs to the own-slice binding.
    void track(const QMetaObject::Connection& c) { m_connections.append(c); }
    // A connection that belongs to the refresh (other slice, active state).
    void trackRefresh(const QMetaObject::Connection& c) { m_refreshConnections.append(c); }

private:
    Sources m_sources;
    QJsonObject m_context;
    std::function<void(SliceModel*)> m_bind;
    std::function<void(SliceModel*)> m_refresh;
    QPointer<SliceModel> m_slice;
    QVector<QMetaObject::Connection> m_connections;
    QVector<QMetaObject::Connection> m_refreshConnections;
    bool m_bound = false;
};

SliceModel* otherSlice(const Sources& sources, const SliceModel* slice)
{
    if (!sources.radio || !slice) { return nullptr; }
    for (SliceModel* s : sources.radio->slices()) {
        if (s && s != slice) { return s; }
    }
    return nullptr;
}

// An inert preview (ContentRenderMode::Preview) shows the slice behind its
// context once and follows nothing.
SliceModel* sliceNow(const Sources& sources, const QJsonObject& context)
{
    return sources.sliceForContext ? sources.sliceForContext(context) : nullptr;
}

QWidget* makeFrequency(const ContentEntry& entry, QWidget* parent, const Sources& sources, bool preview)
{
    auto* w = new FrequencyInstrument(parent);
    const QString form = entry.config.value(QLatin1String(kFormKey)).toString();
    if (form == QLatin1String("bandStrip")) { w->setForm(FrequencyInstrument::Form::BandStrip); }
    else if (form == QLatin1String("flatArc")) { w->setForm(FrequencyInstrument::Form::FlatArc); }
    QPointer<FrequencyInstrument> guard(w);
    SliceFollower* follower = preview ? nullptr : new SliceFollower(w, sources, entry.context);
    auto bind = [guard, follower](SliceModel* slice) {
        if (!guard) { return; }
        guard->setFrequency(slice ? slice->frequency() : 0.0);
        if (!slice || !follower) { return; }
        follower->track(QObject::connect(slice, &SliceModel::frequencyChanged, guard, [guard](double hz) {
            if (guard) { guard->setFrequency(hz); }
        }));
        // The operator's own edit: the slice's existing setter, the same
        // path the VFO flag uses (MainWindow, VfoWidget::frequencyChanged).
        QPointer<SliceModel> target(slice);
        follower->track(QObject::connect(guard, &FrequencyInstrument::frequencyEdited, slice, [target](double hz) {
            if (target) { target->setFrequency(hz); }
        }));
    };
    auto refresh = [guard, follower, sources](SliceModel* slice) {
        if (!guard) { return; }
        SliceModel* other = otherSlice(sources, slice);
        guard->setOtherFrequency(other ? other->frequency() : 0.0);
        // The bound slice's own letter first, then the other slice's.
        // "Other" is otherSlice(): the first slice in RadioModel::slices()
        // that is not this one. With no other slice the second letter is
        // B for slice A and A otherwise (no slice keeps upstream "A"/"B").
        const QChar own = slice ? slice->sliceLetter() : QLatin1Char('A');
        const QChar fallbackOther = own == QLatin1Char('A') ? QLatin1Char('B') : QLatin1Char('A');
        guard->setSliceLetters(own, other ? other->sliceLetter() : fallbackOther);
        guard->setActiveIsThis(!slice || slice->isActiveSlice() || !other || !other->isActiveSlice());
        if (slice && other && follower) {
            follower->trackRefresh(QObject::connect(other, &SliceModel::frequencyChanged, guard, [guard](double hz) {
                if (guard) { guard->setOtherFrequency(hz); }
            }));
        }
    };
    if (follower) {
        follower->start(bind, refresh);
    } else {
        SliceModel* slice = sliceNow(sources, entry.context);
        bind(slice);
        refresh(slice);
    }
    return w;
}

// Frequency (Custom). Absent keys are the defaults: 26 px, locked to
// that size, the amber reading colour, top-left, both slice lines.
void applyFrequencyDisplay(FrequencyInstrument* frequency, const QJsonObject& config)
{
    const int fontPx = config.value(QLatin1String(kFontSizeKey)).toInt(26);
    const bool fit = config.value(QLatin1String(kFitWidthKey)).toBool(false);
    frequency->setDigitFont(fontPx, fit);
    const QString hex = config.value(QLatin1String(kFontColorKey)).toString();
    if (!hex.isEmpty()) {
        const QColor color(hex);
        if (color.isValid()) {
            frequency->setDigitColor(color);
        }
    }
    if (config.value(QLatin1String(kCenterKey)).toBool(false)) {
        frequency->setContentCentered(true);
    }
    const bool own = config.value(QLatin1String(kShowOwnKey)).toBool(true);
    const bool other = config.value(QLatin1String(kShowOtherKey)).toBool(true);
    if (!own || !other) {
        frequency->setSliceLinesVisible(own, other);
    }
}

QWidget* makeFrequencyCustom(const ContentEntry& entry, QWidget* parent, const Sources& sources, bool preview)
{
    QWidget* widget = makeFrequency(entry, parent, sources, preview);
    if (auto* frequency = qobject_cast<FrequencyInstrument*>(widget)) {
        applyFrequencyDisplay(frequency, entry.config);
    }
    return widget;
}

// Task 5b: the poller reads this widget's context on its own timer frame,
// whether or not a MeterWidget shares the context. Called on creation; the
// container host builds a new widget when the source or binding changes,
// and the old one's destruction drops its registration.
// The registration replays the cached readings to this widget alone
// (`replay`), so a sibling on the same context keeps its value and peak.
void registerReadings(MeterPoller* poller, QWidget* widget, const QJsonObject& context,
                      int primary, int secondary, const std::function<void(int, double)>& replay)
{
    QList<int> bindings{primary};
    if (secondary >= 0 && secondary != primary) { bindings.append(secondary); }
    poller->addReadingConsumer(widget, context, bindings, replay);
}

// The readings a needle or bar can show (those with a scale), for the
// property editor; one NereusSDR has no producer for is listed, disabled.
QVector<ContentPropertyChoice> readingChoices(bool withNone)
{
    QVector<ContentPropertyChoice> out;
    if (withNone) {
        out.append({QObject::tr("None"), QJsonValue(QJsonValue::Null), true, {}});
    }
    for (const ReadingDescriptor* d : readingsWithScale()) {
        out.append({d->label, d->bindingId, d->available,
                    d->available ? QString() : QObject::tr("NereusSDR has no source for this reading")});
    }
    return out;
}

QVector<ContentProperty> meterProperties(const QString& typeId)
{
    ContentProperty primary;
    primary.key = QLatin1String(kBindingKey);
    primary.label = QObject::tr("Reading");
    primary.kind = ContentProperty::Kind::Reading;
    primary.choices = readingChoices(false);
    primary.effectiveDefault = [typeId](const QJsonObject& config) -> QJsonValue {
        return primaryBinding(typeId, config);
    };
    ContentProperty secondary;
    secondary.key = QLatin1String(kSecondaryKey);
    secondary.label = typeId == QLatin1String(kBarType) ? QObject::tr("Second reading (second trough)")
                                                         : QObject::tr("Second reading (second pointer)");
    secondary.kind = ContentProperty::Kind::Reading;
    secondary.choices = readingChoices(true);
    secondary.defaultValue = QJsonValue(QJsonValue::Null);
    QVector<ContentProperty> out{primary, secondary};
    if (typeId == QLatin1String(kBarType)) {
        ContentProperty compact;
        compact.key = QLatin1String(kCompactKey);
        compact.label = QObject::tr("Compact (no footer)");
        compact.kind = ContentProperty::Kind::Toggle;
        compact.defaultValue = true;
        out.append(compact);
    }
    return out;
}

QVector<ContentProperty> frequencyProperties()
{
    ContentProperty form;
    form.key = QLatin1String(kFormKey);
    form.label = QObject::tr("Form");
    form.kind = ContentProperty::Kind::Choice;
    form.choices = {{QObject::tr("Number only"), QStringLiteral("numberOnly"), true, {}},
                    {QObject::tr("Band strip"), QStringLiteral("bandStrip"), true, {}},
                    {QObject::tr("Flat arc"), QStringLiteral("flatArc"), true, {}}};
    form.defaultValue = QStringLiteral("numberOnly");
    return {form};
}

QVector<ContentProperty> frequencyDisplayProperties()
{
    QVector<ContentProperty> out = frequencyProperties();

    ContentProperty size;
    size.key = QLatin1String(kFontSizeKey);
    size.label = QObject::tr("Font size");
    size.kind = ContentProperty::Kind::Integer;
    size.minimum = 12;
    size.maximum = 160;
    size.suffix = QObject::tr(" px");
    size.defaultValue = 26;

    ContentProperty fit;
    fit.key = QLatin1String(kFitWidthKey);
    fit.label = QObject::tr("Fit font to the width");
    fit.kind = ContentProperty::Kind::Toggle;
    fit.defaultValue = false;

    ContentProperty color;
    color.key = QLatin1String(kFontColorKey);
    color.label = QObject::tr("Font color");
    color.kind = ContentProperty::Kind::Color;
    color.defaultValue = QStringLiteral("#d8a55f");

    ContentProperty center;
    center.key = QLatin1String(kCenterKey);
    center.label = QObject::tr("Center in the box");
    center.kind = ContentProperty::Kind::Toggle;
    center.defaultValue = false;

    ContentProperty own;
    own.key = QLatin1String(kShowOwnKey);
    own.label = QObject::tr("A/B line: this slice");
    own.kind = ContentProperty::Kind::Toggle;
    own.defaultValue = true;

    ContentProperty other;
    other.key = QLatin1String(kShowOtherKey);
    other.label = QObject::tr("A/B line: other slice");
    other.kind = ContentProperty::Kind::Toggle;
    other.defaultValue = true;

    out << size << fit << color << center << own << other;
    return out;
}

QVector<ContentProperty> filterProperties()
{
    ContentProperty label;
    label.key = QLatin1String(kLabelKey);
    label.label = QObject::tr("Label");
    label.kind = ContentProperty::Kind::Text;
    label.placeholder = QObject::tr("The source slice, for example RX A");
    return {label};
}

} // namespace

int primaryBinding(const QString& typeId, const QJsonObject& config)
{
    const QJsonValue explicitBinding = config.value(QLatin1String(kBindingKey));
    if (explicitBinding.isDouble()) { return explicitBinding.toInt(); }
    if (typeId == QLatin1String(kBarType)) { return MeterBinding::TxSwr; }
    if (config.value(QLatin1String(kDirectionKey)).toString() == QLatin1String("tx")) { return MeterBinding::TxPower; }
    return MeterBinding::SignalPeak;
}

QVector<float> traceFromRendered(const QVector<float>& pixels, double centreHz,
                                 double bandwidthHz, double lowHz, double highHz,
                                 int points)
{
    if (pixels.size() < 2 || bandwidthHz <= 0.0 || points < 2 || highHz <= lowHz) { return {}; }
    const double startHz = centreHz - bandwidthHz / 2.0;
    const double stopHz = centreHz + bandwidthHz / 2.0;
    if (lowHz < startHz || highHz > stopHz) { return {}; }
    const double pxPerHz = pixels.size() / bandwidthHz;
    const int last = int(pixels.size()) - 1;
    QVector<float> out(points);
    const double step = (highHz - lowHz) / points;
    for (int i = 0; i < points; ++i) {
        const double a = lowHz + step * i;
        const double b = a + step;
        const int p0 = std::clamp(int(std::floor((a - startHz) * pxPerHz)), 0, last);
        const int p1 = std::clamp(int(std::ceil((b - startHz) * pxPerHz)) - 1, p0, last);
        float peak = -std::numeric_limits<float>::infinity();
        for (int p = p0; p <= p1; ++p) { peak = std::max(peak, pixels.at(p)); }
        out[i] = peak;
    }
    return out;
}

void registerAll(ContainerContentRegistry& registry, const Sources& sources)
{
    registry.registerWidgetFactory(QLatin1String(kFrequencyType),
        {QLatin1String(kFrequencyType), QStringLiteral("Frequency (Longpath)"), false, true, {}},
        [sources](const ContentEntry& entry, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return makeFrequency(entry, parent, sources, mode == ContentRenderMode::Preview);
        },
        frequencyProperties());

    registry.registerWidgetFactory(QLatin1String(kFrequencyCustomType),
        {QLatin1String(kFrequencyCustomType), QStringLiteral("Frequency (Custom)"), false, true, {}},
        [sources](const ContentEntry& entry, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            return makeFrequencyCustom(entry, parent, sources, mode == ContentRenderMode::Preview);
        },
        frequencyDisplayProperties());

    const auto meterFactory = [sources](const ContentEntry& entry, QWidget* parent,
                                        ContentRenderMode mode) -> QWidget* {
        const int primary = primaryBinding(entry.typeId, entry.config);
        const QJsonValue secondaryValue = entry.config.value(QLatin1String(kSecondaryKey));
        const int secondary = secondaryValue.isDouble() ? secondaryValue.toInt() : -1;
        const ReadingDescriptor* descriptor = readingFor(primary);
        const bool available = descriptor && descriptor->available;
        QWidget* widget = nullptr;
        std::function<void(int, double)> onReading;
        // The poller's reason the primary reading is unavailable (a Core
        // that does not send it, no transmit source): no value, as a
        // MeterWidget shows the binding disabled.
        auto unavailable = std::make_shared<bool>(false);
        if (entry.typeId == QLatin1String(kBarType)) {
            auto owned = std::make_unique<BarInstrument>(parent);
            owned->setCompact(entry.config.value(QLatin1String(kCompactKey)).toBool(true));
            if (!owned->setPrimary(primary)) { return nullptr; }
            BarInstrument* bar = owned.release();
            if (secondary >= 0) { bar->setSecondary(secondary); }
            QPointer<BarInstrument> guard(bar);
            onReading = [guard, primary, unavailable](int binding, double value) {
                if (!guard) { return; }
                if (isNoMeterReading(value) || (binding == primary && *unavailable)) {
                    if (binding == primary) { guard->clearValue(); }
                    else { guard->clearSecondValue(); }
                    return;
                }
                guard->onReading(binding, value);
            };
            widget = bar;
        } else {
            auto owned = std::make_unique<NeedleInstrument>(parent);
            if (!owned->setPrimary(primary)) { return nullptr; }
            NeedleInstrument* needle = owned.release();
            if (secondary >= 0) { needle->setSecondary(secondary); }
            // No reading yet: offline until the first one arrives, and for
            // good when NereusSDR has no producer for the reading.
            needle->setOffline(true);
            QPointer<NeedleInstrument> guard(needle);
            onReading = [guard, primary, available, unavailable](int binding, double value) {
                if (!guard) { return; }
                if (isNoMeterReading(value) || (binding == primary && *unavailable)) {
                    if (binding == primary) { guard->clearValue(); guard->setOffline(true); }
                    else { guard->clearSecondValue(); }
                    return;
                }
                guard->onReading(binding, value);
                if (binding == primary && available) { guard->setOffline(false); }
            };
            widget = needle;
        }
        // Per-model or amplifier scale belongs to this instance, not the
        // fixed descriptor table. Keep its binding and current value intact.
        if (primary == MeterBinding::TxPower && sources.radio) {
            MainPowerMeter* meter = sources.radio->mainPowerMeter();
            const auto scale = [widget, meter] {
                const ReadingDescriptor d = powerReadingDescriptor(meter->visualLimitW(), meter->scaleTopW());
                if (auto* bar = qobject_cast<BarInstrument*>(widget)) { bar->setPrimaryScale(d); }
                if (auto* needle = qobject_cast<NeedleInstrument*>(widget)) { needle->setPrimaryScale(d); }
            };
            scale();
            if (mode != ContentRenderMode::Preview) {
                QObject::connect(meter, &MainPowerMeter::scaleChanged, widget, scale);
            }
        }
        widget->setProperty("longpathPrimaryBinding", primary);
        if (sources.poller && available && mode == ContentRenderMode::Preview) {
            // Inert preview: the cached readings once, nothing registered.
            *unavailable = !sources.poller->bindingUnavailableReason(primary).isEmpty();
            for (int binding : {primary, secondary}) {
                if (binding < 0) { continue; }
                if (const std::optional<double> value = sources.poller->cachedReading(entry.context, binding)) {
                    onReading(binding, *value);
                }
            }
        } else if (sources.poller && available) {
            const QJsonObject context = entry.context;
            QObject::connect(sources.poller, &MeterPoller::readingUpdated, widget,
                [onReading, context, primary, secondary](const QJsonObject& from, int binding, double value) {
                    if (from != context || (binding != primary && binding != secondary)) { return; }
                    onReading(binding, value);
                });
            *unavailable = !sources.poller->bindingUnavailableReason(primary).isEmpty();
            QObject::connect(sources.poller, &MeterPoller::bindingAvailabilityChanged, widget,
                [onReading, unavailable, primary](int binding, const QString& reason) {
                    if (binding != primary) { return; }
                    *unavailable = !reason.isEmpty();
                    if (*unavailable) { onReading(primary, kNoMeterReadingDbm); }
                });
            // The slice behind the context changed: the old slice's value and
            // peak no longer apply, as MeterPoller::synchronizeRxSource resets
            // a target (MeterWidget::resetRxSource) on the same check.
            QObject::connect(sources.poller, &MeterPoller::rxSourceChanged, widget,
                [onReading, context, primary](const QJsonObject& from) {
                    if (from == context) { onReading(primary, kNoMeterReadingDbm); }
                });
            registerReadings(sources.poller, widget, context, primary, secondary, onReading);
        }
        return widget;
    };
    registry.registerWidgetFactory(QLatin1String(kNeedleType),
        {QLatin1String(kNeedleType), QStringLiteral("Needle meter (Longpath)"), false, true, {}},
        meterFactory, meterProperties(QLatin1String(kNeedleType)));
    registry.registerWidgetFactory(QLatin1String(kBarType),
        {QLatin1String(kBarType), QStringLiteral("Bar meter (Longpath)"), false, true, {}},
        meterFactory, meterProperties(QLatin1String(kBarType)));

    registry.registerWidgetFactory(QLatin1String(kFilterType),
        {QLatin1String(kFilterType), QStringLiteral("Filter passband (Longpath)"), false, true, {}},
        [sources](const ContentEntry& entry, QWidget* parent, ContentRenderMode mode) -> QWidget* {
            auto* pane = new BandwidthFilterPane(parent);
            QPointer<BandwidthFilterPane> guard(pane);
            auto slicePtr = std::make_shared<QPointer<SliceModel>>();
            const bool preview = mode == ContentRenderMode::Preview;
            SliceFollower* follower = preview ? nullptr : new SliceFollower(pane, sources, entry.context);
            const QString label = entry.config.value(QLatin1String(kLabelKey)).toString();
            auto bind = [guard, follower, slicePtr, label](SliceModel* slice) {
                if (!guard) { return; }
                *slicePtr = slice;
                guard->setLabel(label.isEmpty() ? sliceLabel(slice) : label);
                guard->setHasFrequency(slice != nullptr);
                guard->setTrace({});
                if (!slice) { return; }
                guard->setAccent(QColor::fromRgb(ControlRanges::sliceColour(slice->sliceIndex())));
                guard->setVfoFrequency(slice->frequency());
                guard->setFilter(slice->filterLow(), slice->filterHigh());
                if (!follower) { return; }
                follower->track(QObject::connect(slice, &SliceModel::frequencyChanged, guard, [guard](double hz) {
                    if (guard) { guard->setVfoFrequency(hz); }
                }));
                follower->track(QObject::connect(slice, &SliceModel::filterChanged, guard, [guard](int low, int high) {
                    if (guard) { guard->setFilter(low, high); }
                }));
                // The operator's own drag: the slice's existing setter.
                QPointer<SliceModel> target(slice);
                follower->track(QObject::connect(guard, &BandwidthFilterPane::filterChanged, slice, [target](int low, int high) {
                    if (target) { target->setFilter(low, high); }
                }));
                follower->track(QObject::connect(guard, &BandwidthFilterPane::filterCentreChanged, slice, [target](int centre) {
                    if (!target) { return; }
                    const int width = target->filterHigh() - target->filterLow();
                    target->setFilter(centre - width / 2, centre - width / 2 + width);
                }));
            };
            if (preview) {
                // Inert preview: the slice once, no trace timer.
                bind(sliceNow(sources, entry.context));
                return pane;
            }
            follower->start(bind);

            // From Longpath src/gui/applets/BandwidthFilterApplet.cpp:779-837 [@551576e3]
            // 20 Hz. Schneller waere Verschwendung: die Kurve dient dem
            // Augenmass beim Kantenziehen, nicht der Signalsuche — dafuer
            // ist der Panadapter da.
            auto* timer = new QTimer(pane);
            timer->setInterval(kTraceIntervalMs);
            QObject::connect(timer, &QTimer::timeout, pane, [guard, slicePtr, sources]() {
                if (!guard || !guard->isVisible()) { return; }
                SliceModel* s = slicePtr->data();
                SpectrumWidget* pan = (s && sources.spectrumForSlice) ? sources.spectrumForSlice(s) : nullptr;
                const double f = s ? s->frequency() : 0.0;
                if (!pan || f <= 0.0) { guard->setTrace({}); return; }
                const double half = guard->spanHz() / 2.0;
                // ── Eine Stuetzstelle je ZWOELF Bildpunkte ────────
                //
                // Hier stand "je zwei Bildpunkte" — und genau daraus
                // kam die Unruhe, die der Betreiber mit "extrem
                // unruhig" beschrieben hat.
                //
                // Auf seinen OpenHPSDR-Bildern vom 2026-08-23 zaehlt
                // man im Rauschbereich Zacken von rund zwoelf
                // Bildpunkten Abstand: die Vorlage tastet GROB ab und
                // verbindet die Punkte gerade. Bei uns lagen die
                // Punkte sechsmal dichter, also lag zwischen zwei
                // Zacken kaum ein Pixel — aus der Kurve wurde ein
                // flimmerndes Band.
                //
                // Die Spitzen gehen dabei nicht verloren: die Quelle
                // nimmt je Eimer das MAXIMUM, ein Traeger bleibt also
                // in voller Hoehe stehen, nur eben als eine Zacke
                // statt als sechs.
                //
                // ── Zwoelf BILDSCHIRMpunkte, nicht zwoelf Qt-Punkte ──
                //
                // Der Betreiber am 2026-09-17: "der bandfilter könnte
                // noch genauer und besser sein."
                //
                // Die zwoelf waren von seinen OpenHPSDR-Bildern
                // abgezaehlt — Bildschirmpunkte. `pane->width()` zaehlt
                // aber Qt-Punkte, und auf seinem Retina-Schirm ist
                // jeder davon zwei Bildschirmpunkte. Unsere
                // Stuetzstellen lagen also doppelt so weit auseinander
                // wie die der Vorlage: 51 Stueck auf 620 Qt-Punkten,
                // eine je 196 Hz bei 10 kHz Spanne — ein Sprechsignal
                // von 2,8 kHz bestand aus vierzehn Punkten. Jetzt in
                // Bildschirmpunkten gerechnet: derselbe Abstand wie in
                // der Vorlage, auf Retina doppelt so viele Stellen,
                // auf einem 1:1-Schirm unveraendert. Dieselbe
                // Korrektur, die der Panadapter am 2026-08-26 fuer
                // seine Abtastbreite bekommen hat (displayWidth in
                // Geraetepunkten, SpectrumWidget::pushSpectrum).
                //
                // Untergrenze 64 statt 48, Obergrenze 400 statt 200 —
                // die alten Deckel waren fuer die halbe Dichte gesetzt.
                const qreal dpr = guard->devicePixelRatioF();
                const int pts = qBound(
                    kTraceMinPoints, static_cast<int>(std::lround(guard->width() * dpr / kTraceDevicePxPerPoint)),
                    kTraceMaxPoints);
                guard->setTrace(traceFromRendered(pan->renderedPixels(), pan->centerFrequency(),
                                                  pan->bandwidth(), f - half, f + half, pts));
            });
            timer->start();
            return pane;
        },
        filterProperties());
}

} // namespace NereusSDR::LongpathContent
