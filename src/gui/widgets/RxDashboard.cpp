// src/gui/widgets/RxDashboard.cpp
//
// Phase 3Q Sub-PR-5 (E.1) — RxDashboard widget
//
// 2026-04-30 update: removed the RX1 + frequency display (freq lives on
// the VFO Flag), composed badges into three BadgePair slots + one lone
// SQL badge, added per-badge tooltips, and extended the responsive
// behavior to a 3-stage ladder: horizontal → stacked-pairs → drop in
// priority order. Bumps StatusBadge sizing rely on the +2 px font /
// +1 px vertical padding change in StatusBadge.cpp.
//
// 2026-08-02 update (bottom-banner cleanup Task A5): dropped the internal
// 3-stage responsive ladder (reapplyDropPriority + BadgePair stacking) in
// favor of a single dense row — ChromeBarController (Task A8) now owns
// folding and queries badgeForRung() to register each foldable badge
// individually. Also: RxDashboard now follows the ACTIVE slice (rebound
// by MainWindow on activeSliceChanged/sliceAdded) instead of being bound
// once to slice 0, and carries a slice-letter tag so a multi-pan operator
// can tell which slice the row describes. See
// docs/architecture/2026-08-02-bottom-banner-and-pan-menu-design.md §4.2.
//
// 2026-09-23 update (R-R3-40): the NR badge shows the warning colour, and
// the step-back reason as its tooltip, while the Core holds NNR below the
// saved choice (SliceModel::nnrLimitChanged). J.J. Boyd (KG4VCF), with
// Anthropic Claude Code assistance.
//
// 2026-09-29 update (slice control plan Task 13): the slice tag sits in a
// picker, "RX [letter] <state> ▾", that opens the all-slice chooser; with
// no slice it reads "Choose a slice". J.J. Boyd (KG4VCF), with Anthropic
// Claude Code assistance.
//
// Signal mapping verified against SliceModel.h 2026-04-30:
//   agcModeChanged(AGCMode)  nbModeChanged(NbMode)
//   dspModeChanged(DSPMode)  activeNrChanged(NrSlot)  apfEnabledChanged(bool)
//   filterChanged(int,int)   ssqlEnabledChanged(bool)

#include "RxDashboard.h"

#include "models/SliceModel.h"
#include "StatusBadge.h"
#include "gui/StyleConstants.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace NereusSDR {

RxDashboard::RxDashboard(QWidget* parent) : QWidget(parent)
{
    buildUi();
    // Horizontal policy is Fixed, not Preferred (final-fix-wave finding
    // 5). Preferred plus the old setMinimumWidth(60) floor let the
    // outer status-bar QHBoxLayout compress this row down to 60 px under
    // width pressure, directly contradicting design doc §5.1 invariant 1
    // ("Nothing shrinks. Every banner item is present at its natural
    // width or absent."). Individual pills still fold the ordinary Qt
    // way -- ChromeBarController calls setVisible() on them directly via
    // badgeForRung(), which shrinks THIS row's own sizeHint() -- Fixed
    // only stops the OUTER layout from squeezing the row any further
    // below whatever that live, pill-count-dependent sizeHint currently
    // is. The row's own non-pill residual (tag + mode + filter +
    // margins) is registered with ChromeBarController at rung 0 via
    // residualWidth(), so the fold budget now accounts for it honestly
    // instead of silently absorbing pressure the ladder never saw.
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    bindSlice(nullptr);
}

namespace {

// Tooltips that don't depend on the active value — mode/filter/AGC/NR
// generate per-state tooltips inline (see the on*Changed handlers).
constexpr auto kNbTooltip  = "Noise blanker active";
constexpr auto kApfTooltip = "Auto-notch filter active";
constexpr auto kSqlTooltip = "Squelch open";

} // namespace

void RxDashboard::buildUi()
{
    auto* hbox = new QHBoxLayout(this);
    hbox->setContentsMargins(10, 0, 10, 0);
    hbox->setSpacing(4);

    // Slice tag — which slice these readings belong to. Prepended so a
    // multi-pan operator isn't left guessing. Slice control plan Task 13:
    // inside the picker that opens the all-slice chooser (the approved
    // bottom area: "RX [letter] <state> ▾").
    m_picker = new QPushButton(this);
    m_picker->setObjectName(QStringLiteral("rxChooserPicker"));
    m_picker->setAccessibleName(tr("Choose a slice"));
    m_picker->setToolTip(tr("Every slice on this Core: listen in, take control, or make a new "
                            "slice"));
    m_picker->setCursor(Qt::PointingHandCursor);
    m_picker->setStyleSheet(QStringLiteral(
        "QPushButton#rxChooserPicker { background: transparent; border: 1px solid %1;"
        " border-radius: 3px; padding: 0px 4px; }"
        "QPushButton#rxChooserPicker:hover { border-color: %2; }")
        .arg(Style::kBorderSubtle, Style::kAccent));
    auto* picker = new QHBoxLayout(m_picker);
    picker->setContentsMargins(4, 0, 4, 0);
    picker->setSpacing(4);
    // A button's own size hint ignores the labels inside it: the layout
    // sizes the picker to its words.
    picker->setSizeConstraint(QLayout::SetFixedSize);
    auto* rx = new QLabel(tr("RX"), m_picker);
    rx->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(Style::kTextSecondary));
    m_sliceTag = new QLabel(QString(m_sliceLetter), m_picker);
    m_sliceTag->setObjectName(QStringLiteral("rxSliceTag"));
    m_sliceTag->setStyleSheet(QStringLiteral(
        "QLabel { color: #0a0a14; background: %1; border-radius: 3px;"
        " padding: 1px 5px; font-weight: bold; font-size: 11px; }")
        .arg(Style::kAccent));
    m_pickerState = new QLabel(tr("Choose a slice"), m_picker);
    m_pickerState->setObjectName(QStringLiteral("rxChooserState"));
    m_pickerState->setStyleSheet(
        QStringLiteral("color: %1; font-size: 11px;").arg(Style::kTextPrimary));
    auto* arrow = new QLabel(QStringLiteral("▾"), m_picker);
    arrow->setStyleSheet(QStringLiteral("color: %1;").arg(Style::kTextSecondary));
    for (QLabel* l : {rx, m_sliceTag, m_pickerState, arrow}) {
        l->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        picker->addWidget(l);
    }
    connect(m_picker, &QPushButton::clicked, this, &RxDashboard::chooserRequested);
    hbox->addWidget(m_picker);

    // Build all badges.
    m_modeBadge   = new StatusBadge(this);
    m_filterBadge = new StatusBadge(this);
    m_agcBadge    = new StatusBadge(this);
    m_nrBadge     = new StatusBadge(this);
    m_nbBadge     = new StatusBadge(this);
    m_apfBadge    = new StatusBadge(this);
    m_sqlBadge    = new StatusBadge(this);

    // Single dense row, no BadgePair wrappers:
    // [slice tag] [mode] [filter] | [AGC] | [NR] [NB] [APF] [SQL]
    hbox->addWidget(m_modeBadge);
    hbox->addWidget(m_filterBadge);
    hbox->addWidget(m_agcBadge);
    hbox->addWidget(m_nrBadge);
    hbox->addWidget(m_nbBadge);
    hbox->addWidget(m_apfBadge);
    hbox->addWidget(m_sqlBadge);

    // R-R3-21: a click opens the VFO flag tab that holds the setting.
    const std::pair<StatusBadge*, Badge> clickable[] = {
        {m_modeBadge, Badge::Mode}, {m_filterBadge, Badge::Filter},
        {m_agcBadge, Badge::Agc},   {m_nrBadge, Badge::Nr},
        {m_nbBadge, Badge::Nb},     {m_apfBadge, Badge::Apf},
        {m_sqlBadge, Badge::Squelch},
    };
    for (const auto& [badge, which] : clickable) {
        badge->setClickable(true);
        connect(badge, &StatusBadge::clicked, this,
                [this, which = which]() {
                    if (m_slice) { emit badgeClicked(which); }
                });
    }

    // Active-only badges hidden by default — "no NYI" rule.
    m_nrBadge->setVisible(false);
    m_nbBadge->setVisible(false);
    m_apfBadge->setVisible(false);
    m_sqlBadge->setVisible(false);

    // SVG icon prefixes — original cc55bde plan called for these glyphs
    // but they were dropped because the Unicode source (⨎ ⚡ ⌁ ∼ ⊘ ▼)
    // rendered inconsistently across font fallbacks. With the SVG icon
    // path now available on StatusBadge (see setSvgIcon docs), each
    // badge gets a font-independent shape that auto-tints to the
    // variant color. The mode badge gets a generic sine-wave glyph —
    // its text label (USB / LSB / CW / AM / FM / DIGU / DIGL) tells you
    // which mode; the icon just marks "this badge is the mode badge".
    m_modeBadge  ->setSvgIcon(QStringLiteral(":/icons/badge-mode.svg"));
    m_filterBadge->setSvgIcon(QStringLiteral(":/icons/badge-filter.svg"));
    m_agcBadge   ->setSvgIcon(QStringLiteral(":/icons/badge-agc.svg"));
    m_nrBadge    ->setSvgIcon(QStringLiteral(":/icons/badge-nr.svg"));
    m_nbBadge    ->setSvgIcon(QStringLiteral(":/icons/badge-nb.svg"));
    m_apfBadge   ->setSvgIcon(QStringLiteral(":/icons/badge-apf.svg"));
    m_sqlBadge   ->setSvgIcon(QStringLiteral(":/icons/badge-sql.svg"));

    // Always-shown badges: placeholder state until bound.
    m_modeBadge->setLabel(QStringLiteral("–"));
    m_modeBadge->setVariant(StatusBadge::Variant::Info);
    m_modeBadge->setToolTip(tr("Operating mode"));

    m_filterBadge->setLabel(QStringLiteral("–"));
    m_filterBadge->setVariant(StatusBadge::Variant::On);
    m_filterBadge->setToolTip(tr("Filter passband width"));

    m_agcBadge->setLabel(QStringLiteral("–"));
    m_agcBadge->setVariant(StatusBadge::Variant::Info);
    m_agcBadge->setToolTip(tr("AGC mode"));

    // Pre-set the active-only tooltips so they're already correct when
    // the badge first becomes visible.
    m_nbBadge->setToolTip(tr(kNbTooltip));
    m_apfBadge->setToolTip(tr(kApfTooltip));
    m_sqlBadge->setToolTip(tr(kSqlTooltip));
}

void RxDashboard::setChooserState(const QString& words)
{
    if (m_pickerState == nullptr || m_pickerState->text() == words) {
        return;
    }
    m_pickerState->setText(words);
    emit residualWidthChanged();
}

QString RxDashboard::chooserState() const
{
    return m_pickerState ? m_pickerState->text() : QString();
}

void RxDashboard::setSliceLetter(QChar letter)
{
    if (m_sliceLetter == letter) {
        return;
    }
    m_sliceLetter = letter;
    if (m_sliceTag) {
        m_sliceTag->setText(QString(letter));
    }
    emit residualWidthChanged();
}

QString RxDashboard::modeText() const
{
    return m_modeBadge ? m_modeBadge->label() : QString();
}

StatusBadge* RxDashboard::badgeForRung(int rung) const
{
    switch (rung) {
    case 5:  return m_sqlBadge;
    case 6:  return m_apfBadge;
    case 7:  return m_nbBadge;
    case 8:  return m_nrBadge;
    case 9:  return m_agcBadge;
    default: return nullptr;
    }
}

int RxDashboard::residualWidth() const
{
    // Mirrors buildUi(): setContentsMargins(10, 0, 10, 0) and
    // setSpacing(4) with exactly two internal gaps among the three
    // never-folding widgets (tag-mode, mode-filter). AGC is deliberately
    // excluded even though it is also always visible: it is one of the
    // five pills and is already counted at its own rung-9 registration.
    constexpr int kMargins = 20;
    constexpr int kGaps    = 2 * 4;
    int w = kMargins + kGaps;
    // Slice control plan Task 13: the picker holds the slice tag.
    if (m_picker)      { w += m_picker->layout()->sizeHint().width(); }
    if (m_modeBadge)   { w += m_modeBadge->sizeHint().width(); }
    if (m_filterBadge) { w += m_filterBadge->sizeHint().width(); }
    return w;
}

void RxDashboard::bindSlice(SliceModel* slice)
{
    if (m_slice == slice && slice) { return; }
    if (m_slice) {
        disconnect(m_slice, nullptr, this, nullptr);
    }
    m_slice = slice;
    const std::pair<StatusBadge*, int> badges[] = {
        {m_sqlBadge, 5}, {m_apfBadge, 6}, {m_nbBadge, 7},
        {m_nrBadge, 8}, {m_agcBadge, 9},
    };
    for (StatusBadge* badge : {m_modeBadge, m_filterBadge, m_agcBadge,
                                m_nrBadge, m_nbBadge, m_apfBadge, m_sqlBadge}) {
        badge->setEnabled(slice != nullptr);
        badge->setClickable(slice != nullptr);
    }
    m_sliceTag->setVisible(slice != nullptr);
    // Slice control plan Task 13: the picker's default words; a window that
    // shares slices sets its own (MainWindow, from the chooser's rows).
    setChooserState(slice != nullptr ? tr("You control") : tr("Choose a slice"));
    if (!slice) {
        setSliceLetter(QChar());
        m_modeBadge->setLabel(QStringLiteral("–"));
        m_modeBadge->setToolTip(tr("Operating mode"));
        m_filterBadge->setLabel(QStringLiteral("–"));
        m_filterBadge->setToolTip(tr("Filter passband width"));
        m_agcBadge->setLabel(QStringLiteral("–"));
        m_agcBadge->setToolTip(tr("AGC mode"));
        for (const auto& [badge, rung] : badges) {
            badge->setLabel(QString());
            emit badgeAvailabilityChanged(rung, false);
        }
        emit residualWidthChanged();
        return;
    }

    connect(slice, &QObject::destroyed, this, [this] { bindSlice(nullptr); });

    // Wire slice signals — exact names verified against SliceModel.h 2026-04-30.
    connect(slice, &SliceModel::dspModeChanged,    this,
            [this](DSPMode m) { onModeChanged(static_cast<int>(m)); });
    connect(slice, &SliceModel::filterChanged,     this,
            [this](int low, int high) { onFilterChanged(low, high); });
    connect(slice, &SliceModel::agcModeChanged,    this,
            [this](AGCMode m) { onAgcChanged(static_cast<int>(m)); });
    connect(slice, &SliceModel::activeNrChanged,   this,
            [this](NrSlot s) { onNrChanged(static_cast<int>(s)); });
    connect(slice, &SliceModel::nnrLimitChanged,   this,
            &RxDashboard::onNnrLimitChanged);
    connect(slice, &SliceModel::nbModeChanged,     this,
            [this](NbMode m) { onNbChanged(static_cast<int>(m)); });
    connect(slice, &SliceModel::apfEnabledChanged, this,
            &RxDashboard::onApfChanged);
    connect(slice, &SliceModel::ssqlEnabledChanged, this,
            &RxDashboard::onSsqlChanged);

    // Seed every badge from the new slice so the row is correct before the
    // first signal arrives (slice may already have state before binding).
    onModeChanged(static_cast<int>(slice->dspMode()));
    onFilterChanged(slice->filterLow(), slice->filterHigh());
    onAgcChanged(static_cast<int>(slice->agcMode()));
    onNrChanged(static_cast<int>(slice->activeNr()));
    onNbChanged(static_cast<int>(slice->nbMode()));
    onApfChanged(slice->apfEnabled());
    onSsqlChanged(slice->ssqlEnabled());
}

void RxDashboard::onModeChanged(int mode)
{
    // Use SliceModel::modeName() static helper (verified present in SliceModel.h).
    const QString name = SliceModel::modeName(static_cast<DSPMode>(mode));
    m_modeBadge->setLabel(name.isEmpty() ? QStringLiteral("–") : name);
    m_modeBadge->setVariant(StatusBadge::Variant::Info);
    m_modeBadge->setToolTip(name.isEmpty()
        ? tr("Operating mode")
        : tr("Mode: %1").arg(name));
    emit residualWidthChanged();
}

void RxDashboard::onFilterChanged(int low, int high)
{
    // Display the total passband width (high − low for SSB/CW, or just high
    // for AM/FM where low ≤ 0). Matches Thetis filter-width display convention.
    const int passband = (low < 0) ? (high - low) : high;
    QString text;
    QString tipDetail;
    if (passband >= 1000) {
        text = QString::asprintf("%.1fk", passband / 1000.0);
        tipDetail = tr("%1 kHz").arg(QString::number(passband / 1000.0, 'f', 1));
    } else if (passband > 0) {
        text = QString::number(passband);
        tipDetail = tr("%1 Hz").arg(passband);
    } else {
        text = QStringLiteral("–");
    }
    m_filterBadge->setLabel(text);
    m_filterBadge->setVariant(StatusBadge::Variant::On);
    m_filterBadge->setToolTip(tipDetail.isEmpty()
        ? tr("Filter passband width")
        : tr("Filter passband: %1").arg(tipDetail));
    emit residualWidthChanged();
}

void RxDashboard::onAgcChanged(int agcMode)
{
    // AGCMode: Off=0, Long=1, Slow=2, Med=3, Fast=4, Custom=5
    // Display single-letter abbreviation: O / L / S / M / F / C
    static const char* kAgcLetters[] = { "O", "L", "S", "M", "F", "C" };
    static const char* kAgcNames[]   = {
        "Off", "Long", "Slow", "Medium", "Fast", "Custom"
    };
    constexpr int kAgcCount = static_cast<int>(
        sizeof(kAgcLetters) / sizeof(kAgcLetters[0]));
    const QString letter = (agcMode >= 0 && agcMode < kAgcCount)
        ? QString::fromLatin1(kAgcLetters[agcMode])
        : QStringLiteral("–");
    const QString full = (agcMode >= 0 && agcMode < kAgcCount)
        ? QString::fromLatin1(kAgcNames[agcMode])
        : QStringLiteral("–");
    m_agcBadge->setLabel(letter);
    m_agcBadge->setVariant(StatusBadge::Variant::Info);
    m_agcBadge->setToolTip(tr("AGC %1").arg(full));
    // AGC has no "off"/hidden state (always available); reported anyway
    // so MainWindow re-checks the badge's width, which setLabel() above
    // just changed live (StatusBadge::setLabel).
    emit badgeAvailabilityChanged(9, true);
}

void RxDashboard::onNrChanged(int nrSlot)
{
    // NrSlot: Off=0, NR1=1, NR2=2, NR3=3, NR4=4, DFNR=5, BNR=6, MNR=7, NNR=8
    if (nrSlot <= 0) {
        emit badgeAvailabilityChanged(8, false);
        return;
    }
    static const char* kNrLabels[] = {
        "Off", "NR1", "NR2", "NR3", "NR4", "DFNR", "BNR", "MNR", "NNR"
    };
    constexpr int kNrCount = static_cast<int>(
        sizeof(kNrLabels) / sizeof(kNrLabels[0]));
    const QString name = (nrSlot > 0 && nrSlot < kNrCount)
        ? QString::fromLatin1(kNrLabels[nrSlot])
        : QStringLiteral("NR");
    m_nrBadge->setLabel(name);
    // The slice words the reason for this window: a remote window names
    // the Core computer, a local one this computer.
    const QString limitReason = nrSlot == static_cast<int>(NrSlot::NNR) && m_slice
        ? m_slice->nnrLimitText() : QString();
    m_nrBadge->setVariant(limitReason.isEmpty() ? StatusBadge::Variant::On
                                                : StatusBadge::Variant::Warn);
    m_nrBadge->setToolTip(limitReason.isEmpty()
        ? tr("Noise reduction %1 active").arg(name) : limitReason);
    emit badgeAvailabilityChanged(8, true);
}

void RxDashboard::onNnrLimitChanged(int /*limit*/)
{
    if (m_slice) {
        onNrChanged(static_cast<int>(m_slice->activeNr()));
    }
}

void RxDashboard::onNbChanged(int nbMode)
{
    // NbMode: Off=0, NB=1, NB2=2
    if (nbMode <= 0) {
        emit badgeAvailabilityChanged(7, false);
        return;
    }
    const QString label = (nbMode == 2)
        ? QStringLiteral("NB2")
        : QStringLiteral("NB");
    m_nbBadge->setLabel(label);
    m_nbBadge->setVariant(StatusBadge::Variant::On);
    m_nbBadge->setToolTip(tr("Noise blanker %1 active").arg(label));
    emit badgeAvailabilityChanged(7, true);
}

void RxDashboard::onApfChanged(bool active)
{
    if (!active) {
        emit badgeAvailabilityChanged(6, false);
        return;
    }
    m_apfBadge->setLabel(QStringLiteral("APF"));
    m_apfBadge->setVariant(StatusBadge::Variant::On);
    m_apfBadge->setToolTip(tr(kApfTooltip));
    emit badgeAvailabilityChanged(6, true);
}

void RxDashboard::onSsqlChanged(bool active)
{
    if (!active) {
        emit badgeAvailabilityChanged(5, false);
        return;
    }
    m_sqlBadge->setLabel(QStringLiteral("SQL"));
    m_sqlBadge->setVariant(StatusBadge::Variant::On);
    m_sqlBadge->setToolTip(tr(kSqlTooltip));
    emit badgeAvailabilityChanged(5, true);
}

} // namespace NereusSDR
