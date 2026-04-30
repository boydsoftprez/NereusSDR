// src/gui/widgets/RxDashboard.cpp
//
// Phase 3Q Sub-PR-5 (E.1) — RxDashboard widget
//
// Dense glance dashboard for RX1 state. Always-visible: frequency, mode,
// filter width, AGC mode. Active-only (hidden when off): NR slot, NB mode,
// APF (auto-notch), soft-squelch.
//
// Signal mapping verified against SliceModel.h 2026-04-30:
//   frequencyChanged(double)   agcModeChanged(AGCMode)  nbModeChanged(NbMode)
//   dspModeChanged(DSPMode)    activeNrChanged(NrSlot)  apfEnabledChanged(bool)
//   filterChanged(int,int)                               ssqlEnabledChanged(bool)

#include "RxDashboard.h"

#include "models/SliceModel.h"
#include "StatusBadge.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QVBoxLayout>

namespace NereusSDR {

RxDashboard::RxDashboard(QWidget* parent) : QWidget(parent)
{
    buildUi();
    // Dashboard min: freq stack 120 + USB ~36 + 3.0k ~46 + S ~24 + 3 spacings
    // 18 + margins 20 ≈ 264 px. Bumped back to 280 (was briefly 220 when
    // working around ADC overload's fixed-width 180 px reservation —
    // that's now hidden-when-idle so the dashboard can claim its full
    // sizeHint without pushing other widgets around).
    setMinimumWidth(280);
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
}

void RxDashboard::buildUi()
{
    auto* hbox = new QHBoxLayout(this);
    hbox->setContentsMargins(10, 0, 10, 0);
    hbox->setSpacing(6);

    // ── Frequency stack: "RX1" label above the freq readout ──────────────────
    auto* stackContainer = new QWidget(this);
    auto* vbox = new QVBoxLayout(stackContainer);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    m_rxLabel = new QLabel(QStringLiteral("RX1"), stackContainer);
    m_rxLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #5fa8ff; font-family: 'SF Mono', Menlo, monospace;"
        " font-size: 9px; font-weight: 600; letter-spacing: 1px; }"));
    m_freqLabel = new QLabel(QStringLiteral("—"), stackContainer); // em dash
    m_freqLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #c8d8e8; font-family: 'SF Mono', Menlo, monospace;"
        " font-size: 15px; font-weight: 600; letter-spacing: 0.3px; }"));
    // Minimum width to prevent truncation of "14.225.000" (12 chars × ~9 px ≈ 110 px).
    // Setting minWidth on the QLabel alone wasn't enough — the parent stackContainer
    // also needs the constraint or the QHBoxLayout in the status bar squeezes both.
    m_freqLabel->setMinimumWidth(120);
    m_freqLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    vbox->addWidget(m_rxLabel);
    vbox->addWidget(m_freqLabel);
    stackContainer->setMinimumWidth(120);
    stackContainer->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    hbox->addWidget(stackContainer);

    // ── Always-visible badges: mode / filter / AGC ───────────────────────────
    // Icons (⨎ ⚡ ⌁ ∼ ⊘ ▼) dropped: these glyphs are absent in Menlo (the
    // SF Mono fallback on macOS without SF Mono installed) and render as
    // garbage. The badge text labels (USB, 2.4k, AGC-S, NR2, NB, APF, SQL)
    // are fully self-descriptive — icon prefixes are redundant.
    m_modeBadge = new StatusBadge(this);
    hbox->addWidget(m_modeBadge);

    m_filterBadge = new StatusBadge(this);
    hbox->addWidget(m_filterBadge);

    m_agcBadge = new StatusBadge(this);
    hbox->addWidget(m_agcBadge);

    // ── Active-only badges: NR / NB / APF / SQL ───────────────────────────────
    m_nrBadge = new StatusBadge(this);
    hbox->addWidget(m_nrBadge);

    m_nbBadge = new StatusBadge(this);
    hbox->addWidget(m_nbBadge);

    m_apfBadge = new StatusBadge(this);
    hbox->addWidget(m_apfBadge);

    m_sqlBadge = new StatusBadge(this);
    hbox->addWidget(m_sqlBadge);

    // Active-only badges hidden by default — "no NYI" rule
    m_nrBadge->setVisible(false);
    m_nbBadge->setVisible(false);
    m_apfBadge->setVisible(false);
    m_sqlBadge->setVisible(false);

    // Always-shown badges: placeholder state until bound
    m_modeBadge->setLabel(QStringLiteral("—"));
    m_modeBadge->setVariant(StatusBadge::Variant::Info);
    m_filterBadge->setLabel(QStringLiteral("—"));
    m_filterBadge->setVariant(StatusBadge::Variant::On);
    m_agcBadge->setLabel(QStringLiteral("—"));
    m_agcBadge->setVariant(StatusBadge::Variant::Info);
}

void RxDashboard::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    reapplyDropPriority();
}

void RxDashboard::reapplyDropPriority()
{
    // Re-entry guard: setVisible() triggers a layout update which can re-fire
    // resizeEvent before this method unwinds. Without the guard, the recursive
    // call iterates m_droppedBadges while the outer call is still mutating it,
    // crashing on iterator invalidation (segfault observed at PID 66366).
    if (m_inReapplyDropPriority) { return; }
    m_inReapplyDropPriority = true;

    // First: try to restore any badges we previously force-dropped, IF
    // their feature is still active (i.e., the host hasn't hidden them
    // for a feature-off reason — those should stay hidden). Take a local
    // copy so the iteration is immune to concurrent modification (defence
    // in depth on top of the re-entry guard).
    const QSet<StatusBadge*> dropped = m_droppedBadges;
    m_droppedBadges.clear();
    for (auto* b : dropped) {
        if (b) { b->setVisible(true); }
    }

    // If we're now over budget, drop badges in priority order until we fit.
    const int budget       = width();
    int       contentWidth = sizeHint().width();
    if (contentWidth > budget) {
        // Drop priority: SQL → APF(ANF) → NB → NR → AGC. Filter never drops.
        StatusBadge* dropOrder[] = {
            m_sqlBadge, m_apfBadge, m_nbBadge, m_nrBadge, m_agcBadge
        };
        for (auto* b : dropOrder) {
            if (contentWidth <= budget) { break; }
            if (b && b->isVisible()) {
                contentWidth -= (b->sizeHint().width() + 6);
                b->setVisible(false);
                m_droppedBadges.insert(b);
            }
        }
    }

    m_inReapplyDropPriority = false;
}

void RxDashboard::bindSlice(SliceModel* slice)
{
    if (m_slice == slice) { return; }
    if (m_slice) {
        disconnect(m_slice, nullptr, this, nullptr);
    }
    m_slice = slice;
    if (!m_slice) { return; }

    // Wire slice signals — exact names verified against SliceModel.h 2026-04-30.
    connect(slice, &SliceModel::frequencyChanged,  this,
            [this](double hz) { onFrequencyChanged(hz); });
    connect(slice, &SliceModel::dspModeChanged,    this,
            [this](DSPMode m) { onModeChanged(static_cast<int>(m)); });
    connect(slice, &SliceModel::filterChanged,     this,
            [this](int low, int high) { onFilterChanged(low, high); });
    connect(slice, &SliceModel::agcModeChanged,    this,
            [this](AGCMode m) { onAgcChanged(static_cast<int>(m)); });
    connect(slice, &SliceModel::activeNrChanged,   this,
            [this](NrSlot s) { onNrChanged(static_cast<int>(s)); });
    connect(slice, &SliceModel::nbModeChanged,     this,
            [this](NbMode m) { onNbChanged(static_cast<int>(m)); });
    connect(slice, &SliceModel::apfEnabledChanged, this,
            &RxDashboard::onApfChanged);
    connect(slice, &SliceModel::ssqlEnabledChanged, this,
            &RxDashboard::onSsqlChanged);

    // Prime with current values (slice may already have state before binding)
    onFrequencyChanged(slice->frequency());
    onModeChanged(static_cast<int>(slice->dspMode()));
    onFilterChanged(slice->filterLow(), slice->filterHigh());
    onAgcChanged(static_cast<int>(slice->agcMode()));
    onNrChanged(static_cast<int>(slice->activeNr()));
    onNbChanged(static_cast<int>(slice->nbMode()));
    onApfChanged(slice->apfEnabled());
    onSsqlChanged(slice->ssqlEnabled());
}

void RxDashboard::onFrequencyChanged(double hz)
{
    if (hz <= 0.0) {
        m_freqLabel->setText(QStringLiteral("—"));
        return;
    }
    const qint64 totalHz = static_cast<qint64>(hz + 0.5);
    const qint64 mHz     = totalHz / 1000000;
    const qint64 kHz     = (totalHz / 1000) % 1000;
    const qint64 hzPart  = totalHz % 1000;
    m_freqLabel->setText(
        QString::asprintf("%lld.%03lld.%03lld",
                          static_cast<long long>(mHz),
                          static_cast<long long>(kHz),
                          static_cast<long long>(hzPart)));
}

void RxDashboard::onModeChanged(int mode)
{
    // Use SliceModel::modeName() static helper (verified present in SliceModel.h).
    const QString name = SliceModel::modeName(static_cast<DSPMode>(mode));
    m_modeBadge->setLabel(name.isEmpty() ? QStringLiteral("—") : name);
    m_modeBadge->setVariant(StatusBadge::Variant::Info);
}

void RxDashboard::onFilterChanged(int low, int high)
{
    // Display the total passband width (high − low for SSB/CW, or just high
    // for AM/FM where low ≤ 0). Matches Thetis filter-width display convention.
    const int width = (low < 0) ? (high - low) : high;
    QString text;
    if (width >= 1000) {
        text = QString::asprintf("%.1fk", width / 1000.0);
    } else if (width > 0) {
        text = QString::number(width);
    } else {
        text = QStringLiteral("—");
    }
    m_filterBadge->setLabel(text);
    m_filterBadge->setVariant(StatusBadge::Variant::On);
}

void RxDashboard::onAgcChanged(int agcMode)
{
    // AGCMode: Off=0, Long=1, Slow=2, Med=3, Fast=4, Custom=5
    // Display single-letter abbreviation: O / L / S / M / F / C
    static const char* kAgcLetters[] = { "O", "L", "S", "M", "F", "C" };
    constexpr int kAgcCount = static_cast<int>(
        sizeof(kAgcLetters) / sizeof(kAgcLetters[0]));
    const QString letter = (agcMode >= 0 && agcMode < kAgcCount)
        ? QString::fromLatin1(kAgcLetters[agcMode])
        : QStringLiteral("—");
    m_agcBadge->setLabel(letter);
    m_agcBadge->setVariant(StatusBadge::Variant::Info);
}

void RxDashboard::onNrChanged(int nrSlot)
{
    // NrSlot: Off=0, NR1=1, NR2=2, NR3=3, NR4=4, DFNR=5, BNR=6, MNR=7
    if (nrSlot <= 0) {
        m_droppedBadges.remove(m_nrBadge);   // feature turned off — clear drop status
        m_nrBadge->setVisible(false);
        return;
    }
    static const char* kNrLabels[] = {
        "Off", "NR1", "NR2", "NR3", "NR4", "DFNR", "BNR", "MNR"
    };
    constexpr int kNrCount = static_cast<int>(
        sizeof(kNrLabels) / sizeof(kNrLabels[0]));
    const QString name = (nrSlot > 0 && nrSlot < kNrCount)
        ? QString::fromLatin1(kNrLabels[nrSlot])
        : QStringLiteral("NR");
    m_nrBadge->setLabel(name);
    m_nrBadge->setVariant(StatusBadge::Variant::On);
    m_droppedBadges.remove(m_nrBadge);   // feature explicitly turned on — clear drop
    m_nrBadge->setVisible(true);
    reapplyDropPriority();   // budget may still be tight; drop something else if needed
}

void RxDashboard::onNbChanged(int nbMode)
{
    // NbMode: Off=0, NB=1, NB2=2
    if (nbMode <= 0) {
        m_droppedBadges.remove(m_nbBadge);   // feature turned off — clear drop status
        m_nbBadge->setVisible(false);
        return;
    }
    const QString label = (nbMode == 2)
        ? QStringLiteral("NB2")
        : QStringLiteral("NB");
    m_nbBadge->setLabel(label);
    m_nbBadge->setVariant(StatusBadge::Variant::On);
    m_droppedBadges.remove(m_nbBadge);   // feature explicitly turned on — clear drop
    m_nbBadge->setVisible(true);
    reapplyDropPriority();   // budget may still be tight; drop something else if needed
}

void RxDashboard::onApfChanged(bool active)
{
    if (!active) {
        m_droppedBadges.remove(m_apfBadge);   // feature turned off — clear drop status
        m_apfBadge->setVisible(false);
        return;
    }
    m_apfBadge->setLabel(QStringLiteral("APF"));
    m_apfBadge->setVariant(StatusBadge::Variant::On);
    m_droppedBadges.remove(m_apfBadge);   // feature explicitly turned on — clear drop
    m_apfBadge->setVisible(true);
    reapplyDropPriority();   // budget may still be tight; drop something else if needed
}

void RxDashboard::onSsqlChanged(bool active)
{
    if (!active) {
        m_droppedBadges.remove(m_sqlBadge);   // feature turned off — clear drop status
        m_sqlBadge->setVisible(false);
        return;
    }
    m_sqlBadge->setLabel(QStringLiteral("SQL"));
    m_sqlBadge->setVariant(StatusBadge::Variant::On);
    m_droppedBadges.remove(m_sqlBadge);   // feature explicitly turned on — clear drop
    m_sqlBadge->setVisible(true);
    reapplyDropPriority();   // budget may still be tight; drop something else if needed
}

} // namespace NereusSDR
