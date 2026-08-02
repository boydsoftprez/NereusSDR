// src/gui/widgets/AdcOverloadBadge.cpp
// no-port-check: NereusSDR-original Qt widget — see header for rationale.
#include "AdcOverloadBadge.h"

#include <QLabel>
#include <QVBoxLayout>

namespace NereusSDR {

AdcOverloadBadge::AdcOverloadBadge(QWidget* parent) : QWidget(parent)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(8, 1, 8, 1);
    vbox->setSpacing(0);

    m_topLabel = new QLabel(this);
    m_topLabel->setObjectName(QStringLiteral("AdcOverloadBadge_Top"));
    m_topLabel->setAlignment(Qt::AlignHCenter);
    vbox->addWidget(m_topLabel);

    m_bottomLabel = new QLabel(QStringLiteral("OVERLOAD"), this);
    m_bottomLabel->setObjectName(QStringLiteral("AdcOverloadBadge_Bottom"));
    m_bottomLabel->setAlignment(Qt::AlignHCenter);
    vbox->addWidget(m_bottomLabel);

    setAttribute(Qt::WA_StyledBackground, true);
    // Vertical: Fixed at the strip height (46 px); horizontal: Minimum so
    // the badge claims its sizeHint() width and the host's QHBoxLayout
    // can't squeeze the OVERLOAD text below its natural width.
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    applyStyle();

    // Bottom-banner cleanup Task A8 fix: m_topLabel starts empty (no ADC
    // list assigned yet) and its stylesheet names a 'SF Mono' family that
    // this widget's own font stack falls back from. Qt resolves that
    // family's substitution/glyph metrics lazily, on the first PAINT of
    // non-empty text in it -- not on construction, not on an empty
    // string, and not on ensurePolished(). Left alone, that first
    // resolution happens whenever setAdcs() is first called for a real
    // overload, i.e. exactly when an alarm fires, and can change
    // sizeHint().height() by a pixel at that moment -- which, since this
    // badge sits in the fixed-width but NOT fixed-height safety-group row
    // (design §4.5), reflows the whole row's vertical centering and
    // shifts every OTHER safety badge (including TX) by that same pixel.
    // That is exactly the "nothing else moves when an alarm fires"
    // invariant this task's safety slots exist to hold, just on the
    // vertical axis instead of horizontal. Priming m_topLabel with real
    // text once here (matching the widest realistic case, a 3-ADC
    // overload) forces that one-time resolution to happen now, before
    // this badge is ever shown, so the later real setAdcs() call cannot
    // change the row's height. Confirmed via
    // tests/tst_mainwindow_status_bar_safety.cpp
    // safetySlotsHoldGeometryWhenAnAlarmFires, which reproduced a 1 px
    // vertical shift deterministically before this fix.
    m_topLabel->setText(QStringLiteral("ADC0/1/2"));
    m_topLabel->setText(QString());
}

void AdcOverloadBadge::setAdcs(const QString& adcs)
{
    if (m_adcs == adcs) { return; }
    m_adcs = adcs;
    m_topLabel->setText(QStringLiteral("ADC%1").arg(adcs));
    updateGeometry();
}

void AdcOverloadBadge::setVariant(Variant v)
{
    if (m_variant == v) { return; }
    m_variant = v;
    applyStyle();
}

void AdcOverloadBadge::applyStyle()
{
    // ucInfoBar.cs:928 [@501e3f5] — red_warning ? Red : Yellow.
    // Match the StatusBadge palette so the alarm reads as part of the
    // badge family in the right-side strip.
    QString fg, bg;
    switch (m_variant) {
        case Variant::Warn:
            fg = QStringLiteral("#ffd700");
            bg = QStringLiteral("rgba(255,215,0,30)");
            break;
        case Variant::Tx:
            fg = QStringLiteral("#ff6060");
            bg = QStringLiteral("rgba(255,96,96,51)");
            break;
    }

    setStyleSheet(QStringLiteral(
        "NereusSDR--AdcOverloadBadge {"
        " background: %1; border-radius: 3px;"
        "}"
        // Top row: small caps, semi-bold, tight letter-spacing — reads as
        // a label rather than a value.
        "QLabel#AdcOverloadBadge_Top {"
        " color: %2;"
        " font-family: 'SF Mono', Menlo, monospace;"
        " font-size: 9px; font-weight: 600;"
        " letter-spacing: 1px;"
        " background: transparent; border: none;"
        "}"
        // Bottom row: bigger, bold, slightly tighter line height — the
        // alarm word that the user reads first.
        "QLabel#AdcOverloadBadge_Bottom {"
        " color: %2;"
        " font-family: 'SF Mono', Menlo, monospace;"
        " font-size: 11px; font-weight: 800;"
        " letter-spacing: 0.5px;"
        " background: transparent; border: none;"
        "}"
    ).arg(bg, fg));
}

} // namespace NereusSDR
