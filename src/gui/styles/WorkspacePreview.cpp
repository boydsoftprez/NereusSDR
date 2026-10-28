// =================================================================
// src/gui/styles/WorkspacePreview.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See WorkspacePreview.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Container item editors' colour swatches (a sheet that is
//                 only a #AARRGGBB background in the swatch border) keep
//                 the user's colour, as ColorSwatchButton does. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-07 - Written by J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - themeOnShow() installs its filter once per dialog.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - installLiveTheming(): the application-wide live pass.
//                 themeOne() records the themed sheet before it sets it.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The unknown-literal count leaves out the colours the
//                 workspace look itself writes, not only palette values.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - roleColour() and roleColourName() for painted chrome
//                 and rich text. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - The status and data colour choices: statusColour(),
//                 dataBackground() and meterInk(). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-08 - JJ's decisions are the product: the brighter indicator
//                 tones and the container's background, the user's when
//                 set. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-09 - JJ's indicator rules per check box and radio no sheet
//                 styles the indicator of, with the padding that keeps
//                 its size. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-09 - The same measured pass for sliders no sheet styles (the
//                 decided slider) and checkable group boxes (the decided
//                 indicator). J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-09 - sliderBlock() keeps a slider at least as thick as the
//                 decided handle, and so does sliderFit() for a slider
//                 the style or the program left thinner; a thicker
//                 slider keeps its size. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-09 - D28: a slider sliderFit() raises takes its pixels from
//                 the panel: a stack it sits in grows by the same pixels,
//                 and so does a stacked widget whose page grew, so the
//                 widgets beside it keep their size.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-10-10 - A progress bar no sheet styles takes the selection glass
//                 as its filled part (settled with JJ's colour answers),
//                 at the size it has today. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-10 - installLiveTheming() also installs the look's style
//                 object (Workspace::LookStyle): the default button's rim
//                 and the current cell's outline. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-12 - enabled() is on unless NEREUS_WORKSPACE_PREVIEW is "0"
//                 (enabledFromEnvironmentValue()). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-12 - Shared/live 11 px font floor, J.J. Boyd (KG4VCF),
//                 AI-assisted via OpenAI Codex.
// =================================================================

#include "WorkspacePreview.h"

#include "QssTranslator.h"
#include "WorkspaceComponents.h"
#include "WorkspaceLookStyle.h"
#include "WorkspacePalette.h"
#include "WorkspaceStyle.h"

#include <QApplication>
#include <QBoxLayout>
#include <QEvent>
#include <QGroupBox>
#include <QFontMetricsF>
#include <QLabel>
#include <QRegularExpression>
#include <QTextEdit>
#include <QTextDocument>
#include <QTextBlock>
#include <QTextFragment>
#include <QTextCursor>
#include <QScopedValueRollback>
#include <QMetaObject>
#include <QObject>
#include <QPushButton>
#include <QSet>
#include <QStackedWidget>
#include <QStyle>
#include <QStyleOptionGroupBox>
#include <QVariant>
#include <QWidget>
#include <QWindow>

#include <algorithm>
#include <optional>

namespace NereusSDR::WorkspacePreview {

namespace {

constexpr int kMinimumTextPixels = 11;
constexpr qreal kPointsPerInch = 72.0;

std::optional<bool>& switchState()
{
    static std::optional<bool> state;
    return state;
}

QHash<QString, int>& unknownCounts()
{
    static QHash<QString, int> counts;
    return counts;
}

// Colours the workspace look writes itself: palette values, the
// colours componentize() derives from them, and the workspace sheets'
// own literals. A sheet the program copies from an already themed
// widget brings them back to the translator; they are not legacy
// literals.
const QSet<QString>& workspaceValues()
{
    static const QSet<QString> values = [] {
        QSet<QString> s;
        for (const auto& e : Workspace::WorkspacePalette::glassAndDepth().entries()) {
            s.insert(QString::fromLatin1(e.hex).toLower());
        }
        for (const QString& v : Workspace::Components::generatedColours()) {
            s.insert(v);
        }
        const QssTranslator::Table noRows;
        const QssTranslator::KeptList noKept;
        QssTranslator::Options options;
        options.table = &noRows;
        options.kept = &noKept;
        const QString sheets = QString::fromLatin1(Workspace::kScrollBarStyle)
            + Workspace::titleBarStyle();
        for (const QString& v : QssTranslator::translate(sheets, options).unknownLiterals) {
            s.insert(v);
        }
        return s;
    }();
    return values;
}

// Class names whose subtree the scoped pass leaves alone.
//  - VfoLevelBar: the slice flag's S-meter bar, a data surface. The flag
//    itself and its other helpers take the look (JJ, 2026-10-11; before
//    that the whole flag was left out, design-facts B11).
//  - SpectrumWidget: plot colors and filter overlays live there; its
//    chrome is themed at call sites. The flag and its floating buttons
//    are its children and opt in with kThemedIslandProperty.
//  - Colour pickers whose stylesheet shows a user-chosen colour.
constexpr const char* kSkippedClasses[] = {
    "NereusSDR::VfoLevelBar",
    "NereusSDR::SpectrumWidget",
    "NereusSDR::ColorSwatchButton",
    "NereusSDR::GradientPickerWidget",
};

bool skippedClass(const QMetaObject* meta)
{
    // The live pass asks this for every ancestor of every widget shown,
    // so the answer is kept per class.
    static QHash<const QMetaObject*, bool> known;
    const auto it = known.constFind(meta);
    if (it != known.constEnd()) {
        return it.value();
    }
    bool skip = false;
    for (const QMetaObject* m = meta; m != nullptr && !skip; m = m->superClass()) {
        for (const char* cls : kSkippedClasses) {
            if (qstrcmp(m->className(), cls) == 0) {
                skip = true;
                break;
            }
        }
    }
    known.insert(meta, skip);
    return skip;
}

// A container item editor's colour swatch: a push button whose own sheet
// is only the colour the user chose (#AARRGGBB) inside the swatch border,
// "QPushButton { background: %1; border: 1px solid #205070; }" (Clock,
// LED, Text overlay and the other meter_property_editors). Like a
// ColorSwatchButton it shows a user colour, which the look never
// translates: a grey or white the item draws must read as that colour.
bool colourSwatchSheet(const QWidget* w)
{
    if (qobject_cast<const QPushButton*>(w) == nullptr) {
        return false;
    }
    static const QRegularExpression swatch(QStringLiteral(
        "^\\s*QPushButton\\s*\\{\\s*background:\\s*#[0-9A-Fa-f]{8};\\s*border:\\s*1px solid #205070;\\s*\\}\\s*$"));
    return swatch.match(w->styleSheet()).hasMatch();
}

bool skipped(const QWidget* w)
{
    if (w->property(kSkipProperty).toBool()) {
        return true;
    }
    if (colourSwatchSheet(w)) {
        return true;
    }
    return skippedClass(w->metaObject());
}

// True when `w` or an ancestor in its own window is skipped. The walk
// stops at the window: a menu or popup a skipped widget opens is a
// surface of its own. It also stops at a widget carrying
// kThemedIslandProperty: that widget and what it holds take the look
// whatever is above it.
bool skippedInWindow(const QWidget* w)
{
    for (const QWidget* p = w; p != nullptr; p = p->parentWidget()) {
        if (skipped(p)) {
            return true;
        }
        if (p->property(kThemedIslandProperty).toBool()) {
            break;
        }
        if (p->isWindow()) {
            break;
        }
    }
    return false;
}

// The indicator block a check box or radio gets when no sheet above it
// styles its indicator.
constexpr auto kIndicatorBlockProperty = "_nereusWorkspaceIndicatorBlock";

bool isToggle(const QWidget* w)
{
    return w->inherits("QCheckBox") || w->inherits("QRadioButton");
}

// True when the box's own sheet, an ancestor's or the application's
// styles an indicator: that sheet sized the box already.
bool indicatorStyled(const QWidget* w, const QString& own)
{
    const QLatin1String sub("::indicator");
    if (own.contains(sub, Qt::CaseInsensitive)) {
        return true;
    }
    for (const QWidget* p = w->parentWidget(); p != nullptr; p = p->parentWidget()) {
        if (p->styleSheet().contains(sub, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return qApp != nullptr && qApp->styleSheet().contains(sub, Qt::CaseInsensitive);
}

// JJ's indicator rules for a box no sheet styles the indicator of, with
// what keeps its size: an indicator rule makes Qt's stylesheet style size
// the box as text, spacing and indicator only, without the label margins
// the platform style adds (and with the spacing even when there is no
// text). The box is measured as it is (its sheet without the rules: today's
// size) and with the rules, and the difference goes back as padding, or
// as less spacing where the rules made it wider. `look` is the box's own
// themed sheet; `current` is the sheet it has now.
QString indicatorBlock(QWidget* w, const QString& look, const QString& current)
{
    const bool radio = w->inherits("QRadioButton");
    const QString cls = radio ? QStringLiteral("QRadioButton") : QStringLiteral("QCheckBox");
    const QString rules = Workspace::Components::indicatorRulesFor(radio);
    const QSize today = w->sizeHint();
    // Recorded first, so the live pass leaves the measuring sheet alone.
    const auto measure = [w](const QString& sheet) {
        w->setProperty(kThemedQssProperty, sheet);
        w->setStyleSheet(sheet);
        return w->sizeHint();
    };
    const QSize bare = measure(look + rules);
    int dw = today.width() - bare.width();
    const int dh = std::max(0, today.height() - bare.height());
    QString decls;
    if (dw < 0) {
        const int spacing = w->style()->pixelMetric(
            radio ? QStyle::PM_RadioButtonLabelSpacing : QStyle::PM_CheckBoxLabelSpacing, nullptr, w);
        const int less = std::min(spacing, -dw);
        decls += QStringLiteral(" spacing: %1px;").arg(spacing - less);
        dw += less;
    }
    if (dw > 0 || dh > 0) {
        decls += QStringLiteral(" padding: %1px %2px %3px 0px;").arg(dh / 2).arg(std::max(0, dw)).arg(dh - dh / 2);
    }
    // Put back what the box had; the caller sets the themed sheet.
    w->setProperty(kThemedQssProperty, current);
    w->setStyleSheet(current);
    if (decls.isEmpty()) {
        return rules;
    }
    return rules + QStringLiteral("%1 {%2 }\n").arg(cls, decls);
}

// True when a sheet on the slider, an ancestor or the application styles
// a slider: that sheet drew it already.
bool sliderStyled(const QWidget* w, const QString& own)
{
    // An ancestor's or the application's "::handle" is a scroll bar's
    // (the workspace scroll bar); only the slider's own sheet counts it.
    const auto styles = [](const QString& sheet) {
        return sheet.contains(QLatin1String("QSlider"), Qt::CaseInsensitive);
    };
    if (styles(own) || own.contains(QLatin1String("::groove"), Qt::CaseInsensitive)
        || own.contains(QLatin1String("::handle"), Qt::CaseInsensitive)) {
        return true;
    }
    for (const QWidget* p = w->parentWidget(); p != nullptr; p = p->parentWidget()) {
        if (styles(p->styleSheet())) {
            return true;
        }
    }
    return qApp != nullptr && styles(qApp->styleSheet());
}

bool isUnstyledSlider(const QWidget* w, const QString& own)
{
    return w->inherits("QSlider") && !sliderStyled(w, own);
}

// True when a sheet on the group box, an ancestor or the application
// styles a group box's indicator.
bool groupIndicatorStyled(const QWidget* w, const QString& own)
{
    if (own.contains(QLatin1String("::indicator"), Qt::CaseInsensitive)) {
        return true;
    }
    const QLatin1String sub("QGroupBox::indicator");
    for (const QWidget* p = w->parentWidget(); p != nullptr; p = p->parentWidget()) {
        if (p->styleSheet().contains(sub, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return qApp != nullptr && qApp->styleSheet().contains(sub, Qt::CaseInsensitive);
}

bool isUnstyledCheckableGroup(const QWidget* w, const QString& own)
{
    const auto* g = qobject_cast<const QGroupBox*>(w);
    return g != nullptr && g->isCheckable() && !groupIndicatorStyled(w, own);
}

// The decided slider for a slider no sheet styles, with what keeps its
// size: Longpath's groove and handle change the slider's size hint
// across its length, so the slider is measured as it is and with the
// rules, and today's thickness goes back as a minimum and maximum.
// A thickness the program fixed stays as it is.
QString sliderBlock(QWidget* w, const QString& look, const QString& current)
{
    const bool vertical = w->property("orientation").toInt() == Qt::Vertical;
    const QString rules = Workspace::Components::sliderRulesFor(vertical);
    const QSize today = w->sizeHint();
    const auto measure = [w](const QString& sheet) {
        w->setProperty(kThemedQssProperty, sheet);
        w->setStyleSheet(sheet);
        return w->sizeHint();
    };
    const QSize bare = measure(look + rules);
    QString decls;
    // Never thinner than the decided handle, or the handle is cut off.
    const int span = Workspace::Components::kSliderHandleSpanPx;
    if (vertical) {
        if (bare.width() != today.width() && w->minimumWidth() != w->maximumWidth()) {
            decls += QStringLiteral(" min-width: %1px; max-width: %1px;").arg(qMax(today.width(), span));
        }
    } else if (bare.height() != today.height() && w->minimumHeight() != w->maximumHeight()) {
        decls += QStringLiteral(" min-height: %1px; max-height: %1px;").arg(qMax(today.height(), span));
    }
    w->setProperty(kThemedQssProperty, current);
    w->setStyleSheet(current);
    if (decls.isEmpty()) {
        return rules;
    }
    return rules + QStringLiteral("QSlider {%1 }\n").arg(decls);
}

// True when a sheet on the progress bar, an ancestor or the application
// styles a progress bar: that sheet drew it already.
bool isUnstyledProgressBar(const QWidget* w, const QString& own)
{
    if (!w->inherits("QProgressBar")) {
        return false;
    }
    const auto styles = [](const QString& sheet) {
        return sheet.contains(QLatin1String("QProgressBar"), Qt::CaseInsensitive);
    };
    if (styles(own) || own.contains(QLatin1String("::chunk"), Qt::CaseInsensitive)) {
        return false;
    }
    for (const QWidget* p = w->parentWidget(); p != nullptr; p = p->parentWidget()) {
        if (styles(p->styleSheet())) {
            return false;
        }
    }
    return qApp == nullptr || !styles(qApp->styleSheet());
}

// The selection glass as the filled part of a progress bar no sheet
// styles (settled 2026-10-10), with what keeps its size: the bar is
// measured as it is and with the rule, and today's thickness goes back
// as a minimum and maximum where the rule changed it. A thickness the
// program fixed stays as it is.
QString progressBlock(QWidget* w, const QString& look, const QString& current)
{
    const bool vertical = w->property("orientation").toInt() == Qt::Vertical;
    const QString rules = Workspace::Components::progressChunkRules();
    const QSize today = w->sizeHint();
    const auto measure = [w](const QString& sheet) {
        w->setProperty(kThemedQssProperty, sheet);
        w->setStyleSheet(sheet);
        return w->sizeHint();
    };
    const QSize bare = measure(look + rules);
    QString decls;
    if (vertical) {
        if (bare.width() != today.width() && w->minimumWidth() != w->maximumWidth()) {
            decls += QStringLiteral(" min-width: %1px; max-width: %1px;").arg(today.width());
        }
    } else if (bare.height() != today.height() && w->minimumHeight() != w->maximumHeight()) {
        decls += QStringLiteral(" min-height: %1px; max-height: %1px;").arg(today.height());
    }
    w->setProperty(kThemedQssProperty, current);
    w->setStyleSheet(current);
    if (decls.isEmpty()) {
        return rules;
    }
    return rules + QStringLiteral("QProgressBar {%1 }\n").arg(decls);
}

// The decided indicator for a checkable group box, at the size the
// group box draws its indicator now. The option is filled as
// QGroupBox::initStyleOption() fills it (protected).
QString groupBoxBlock(QWidget* w)
{
    const auto* g = static_cast<const QGroupBox*>(w);
    QStyleOptionGroupBox o;
    o.initFrom(g);
    o.text = g->title();
    o.lineWidth = 1;
    o.midLineWidth = 0;
    o.textAlignment = g->alignment();
    o.subControls = QStyle::SC_GroupBoxFrame | QStyle::SC_GroupBoxCheckBox;
    if (!g->title().isEmpty()) {
        o.subControls |= QStyle::SC_GroupBoxLabel;
    }
    if (g->isFlat()) {
        o.features |= QStyleOptionFrame::Flat;
    }
    o.state |= g->isChecked() ? QStyle::State_On : QStyle::State_Off;
    const QRect r = w->style()->subControlRect(QStyle::CC_GroupBox, &o, QStyle::SC_GroupBoxCheckBox, w);
    return Workspace::Components::groupBoxIndicatorRules(r.width(), r.height());
}

// A slider thinner than the decided handle: the style's own thickness
// (15 px with Fusion on macOS) or a size the program fixed below it. The
// rule makes it as thick as the handle, and only such a slider gets it:
// a min-height in a sheet also becomes the slider's size hint, so a
// slider already thicker keeps its size. Once given, the rule stays (it
// raises what a later pass would read); until then each pass looks
// again, since the program may fix the size after the slider was first
// seen.
constexpr auto kSliderFitProperty = "_nereusWorkspaceSliderFit";

// D28 (JJ, 2026-10-09): the pixels a raised slider gains come from the
// panel, never from the widgets beside it.
//
// The innermost box layout under `root` that holds `w` itself.
QBoxLayout* boxHolding(QLayout* root, QWidget* w)
{
    if (root == nullptr) {
        return nullptr;
    }
    for (int i = 0; i < root->count(); ++i) {
        QLayoutItem* item = root->itemAt(i);
        if (item->widget() == w) {
            return qobject_cast<QBoxLayout*>(root);
        }
        if (QBoxLayout* inner = boxHolding(item->layout(), w)) {
            return inner;
        }
    }
    return nullptr;
}

bool runsDown(const QBoxLayout* box)
{
    return box->direction() == QBoxLayout::TopToBottom || box->direction() == QBoxLayout::BottomToTop;
}

// The height a row gives `w`: the tallest hint of what shares the row
// with it, or `w`'s own hint when that is taller. A stacked layout counts
// only `w`.
int rowHeight(QWidget* w, int ownHint)
{
    int height = ownHint;
    QWidget* parent = w->parentWidget();
    QBoxLayout* row = parent ? boxHolding(parent->layout(), w) : nullptr;
    if (row == nullptr || runsDown(row)) {
        return height;
    }
    for (int i = 0; i < row->count(); ++i) {
        QLayoutItem* item = row->itemAt(i);
        if (item->widget() == w || (item->widget() != nullptr && item->widget()->isHidden())) {
            continue;
        }
        height = qMax(height, item->sizeHint().height());
    }
    return height;
}

// A horizontal slider raised by `gain` pixels. In a stack (a slider over a
// meter strip, say) the stack would keep the height its row gives it and
// the gain would come out of the widgets stacked with it, so the stack is
// made taller by the gain. In a row the row grows by itself to the
// thicker slider. Either way, a page of a stacked widget that grows gets
// the room from the stacked widget, so spare room the page had (the
// stacked widget is as tall as its tallest page) stays where it was.
// Hint based, so a hidden page grows the same way.
void growForSlider(QWidget* slider, int gain)
{
    QWidget* holder = slider->parentWidget();
    if (gain <= 0 || holder == nullptr) {
        return;
    }
    int growth = 0;
    QWidget* grown = slider;
    QBoxLayout* box = boxHolding(holder->layout(), slider);
    bool neighbour = false;
    if (box != nullptr && runsDown(box) && holder->layout() == box) {
        for (int i = 0; i < box->count(); ++i) {
            QWidget* other = box->itemAt(i)->widget();
            if (other != nullptr && other != slider && !other->isHidden()) {
                neighbour = true;
            }
        }
    }
    if (neighbour) {
        const int before = rowHeight(holder, holder->sizeHint().height());
        holder->setMinimumHeight(qMax(holder->minimumHeight(), before + gain));
        growth = gain;
        grown = holder;
    } else {
        const int thickness = Workspace::Components::kSliderHandleSpanPx - gain;
        growth = rowHeight(slider, Workspace::Components::kSliderHandleSpanPx) - rowHeight(slider, thickness);
    }
    if (growth <= 0) {
        return;
    }
    for (QWidget* a = grown->parentWidget(); a != nullptr && !a->isWindow(); a = a->parentWidget()) {
        if (auto* pages = qobject_cast<QStackedWidget*>(a)) {
            pages->setMinimumHeight(qMax(pages->minimumHeight(), pages->sizeHint().height()) + growth);
        }
    }
}

QString sliderFit(QWidget* w)
{
    if (!w->inherits("QSlider")) {
        return {};
    }
    const QString known = w->property(kSliderFitProperty).toString();
    if (!known.isEmpty()) {
        return known;
    }
    const int span = Workspace::Components::kSliderHandleSpanPx;
    const bool vertical = w->property("orientation").toInt() == Qt::Vertical;
    const QSize hint = w->sizeHint();
    QString fit;
    int thickness = span;
    // The thickness a layout gives the slider: its hint, never below its
    // minimum (a size the program fixed is both).
    if (vertical) {
        if (w->maximumWidth() < span) {
            thickness = w->maximumWidth();
            fit = QStringLiteral("QSlider { min-width: %1px; max-width: %1px; }\n").arg(span);
        } else if (qMax(hint.width(), w->minimumWidth()) < span) {
            thickness = qMax(hint.width(), w->minimumWidth());
            fit = QStringLiteral("QSlider { min-width: %1px; }\n").arg(span);
        }
    } else if (w->maximumHeight() < span) {
        thickness = w->maximumHeight();
        fit = QStringLiteral("QSlider { min-height: %1px; max-height: %1px; }\n").arg(span);
    } else if (qMax(hint.height(), w->minimumHeight()) < span) {
        thickness = qMax(hint.height(), w->minimumHeight());
        fit = QStringLiteral("QSlider { min-height: %1px; }\n").arg(span);
    }
    if (!fit.isEmpty()) {
        w->setProperty(kSliderFitProperty, fit);
        if (!vertical) {
            growForSlider(w, span - thickness);
        }
    }
    return fit;
}

QFont readableFontAtDpi(const QFont& font, qreal logicalDpi)
{
    if (!enabled()) {
        return font;
    }
    const qreal pixels = font.pixelSize() > 0
        ? font.pixelSize()
        : font.pointSizeF() * logicalDpi / kPointsPerInch;
    if (pixels >= kMinimumTextPixels || pixels <= 0) {
        return font;
    }
    QFont readable = font;
    readable.setPixelSize(kMinimumTextPixels);
    return readable;
}

void floorWidgetFont(QWidget* w)
{
    // setFont() delivers FontChange synchronously, including to descendants.
    // The guard prevents a style from re-entering while resolving its font.
    static bool applying = false;
    if (applying) {
        return;
    }
    const QFont current = w->font();
    const QFont readable = readableFontAtDpi(current, QFontMetricsF(current, w).fontDpi());
    if (readable != current) {
        QScopedValueRollback<bool> guard(applying, true);
        w->setFont(readable);
    }
}

void setFontFloorOptions(QssTranslator::Options& options, const QWidget* owner)
{
    options.minimumTextPixelSize = kMinimumTextPixels;
    options.textLogicalDpi = owner != nullptr
        ? QFontMetricsF(owner->font(), owner).fontDpi()
        : QFontMetricsF(QApplication::font()).fontDpi();
}

constexpr auto kRichTextCache = "_nereusWorkspaceReadableRichText";
constexpr auto kRichTextFormatCache = "_nereusWorkspaceReadableRichFormat";
constexpr auto kRichDocumentConnected = "_nereusWorkspaceReadableDocumentConnected";

QString floorRichHtml(const QString& html, const QWidget* owner)
{
    // Consume whole attributes, so style-like words inside another quoted
    // attribute, prose, URLs, escaped code and HTML comments stay untouched.
    static const QRegularExpression tags(QStringLiteral(
        R"re(<!--[\s\S]*?-->|<(?:[^<>"']|"[^"]*"|'[^']*')+>)re"));
    static const QRegularExpression attributes(QStringLiteral(
        R"re(([\w:-]+)\s*=\s*("[^"]*"|'[^']*'))re"));
    QssTranslator::Options options;
    setFontFloorOptions(options, owner);
    options.translateColours = false;
    QString result;
    qsizetype copied = 0;
    QRegularExpressionMatchIterator matches = tags.globalMatch(html);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        result += QStringView(html).mid(copied, match.capturedStart() - copied);
        QString tag = match.captured();
        if (!tag.startsWith(QLatin1String("<!--"))) {
            QRegularExpressionMatchIterator attrs = attributes.globalMatch(tag);
            QVector<QPair<qsizetype, QPair<qsizetype, QString>>> changes;
            while (attrs.hasNext()) {
                const QRegularExpressionMatch attr = attrs.next();
                if (attr.captured(1).compare(QLatin1String("style"), Qt::CaseInsensitive) != 0) {
                    continue;
                }
                const QString value = attr.captured(2).mid(1, attr.capturedLength(2) - 2);
                const QString floored = QssTranslator::translate(value, options).qss;
                if (floored != value) {
                    changes.append({attr.capturedStart(2) + 1,
                                    {attr.capturedLength(2) - 2, floored}});
                }
            }
            for (auto it = changes.crbegin(); it != changes.crend(); ++it) {
                tag.replace(it->first, it->second.first, it->second.second);
            }
        }
        result += tag;
        copied = match.capturedEnd();
    }
    result += QStringView(html).mid(copied);
    return result;
}

void floorRichDocument(QTextEdit* edit)
{
    // Read-only displays can adapt typography without changing an editor's
    // user-authored formatting or its undo history.
    if (!enabled() || !edit->isReadOnly()) {
        return;
    }
    QTextDocument* document = edit->document();
    constexpr auto kApplying = "_nereusWorkspaceApplyingDocumentFont";
    if (document->property(kApplying).toBool()) {
        return;
    }
    struct Change { int start; int length; QTextCharFormat format; };
    QVector<Change> changes;
    const qreal dpi = QFontMetricsF(edit->font(), edit).fontDpi();
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            QTextCharFormat format = fragment.charFormat();
            if (!format.hasProperty(QTextFormat::FontPixelSize)
                && !format.hasProperty(QTextFormat::FontPointSize)) {
                continue;
            }
            const QFont original = format.font();
            if (readableFontAtDpi(original, dpi) == original) {
                continue;
            }
            format.clearProperty(QTextFormat::FontPointSize);
            format.setProperty(QTextFormat::FontPixelSize, kMinimumTextPixels);
            changes.append({fragment.position(), fragment.length(), format});
        }
    }
    if (changes.isEmpty()) {
        return;
    }
    document->setProperty(kApplying, true);
    const bool modified = document->isModified();
    QTextCursor cursor(document);
    cursor.beginEditBlock();
    for (const Change& change : changes) {
        cursor.setPosition(change.start);
        cursor.setPosition(change.start + change.length, QTextCursor::KeepAnchor);
        cursor.setCharFormat(change.format);
    }
    cursor.endEditBlock();
    document->setModified(modified);
    document->setProperty(kApplying, false);
}

void floorRichText(QWidget* w)
{
    if (QLabel* label = qobject_cast<QLabel*>(w)) {
        const QString current = label->text();
        if (label->property(kRichTextCache).isValid()
            && label->property(kRichTextCache).toString() == current
            && label->property(kRichTextFormatCache).toInt() == label->textFormat()) {
            return;
        }
        QString readable = current;
        if (label->textFormat() == Qt::RichText
            || (label->textFormat() == Qt::AutoText && Qt::mightBeRichText(current))) {
            readable = floorRichHtml(current, label);
        }
        // Cache before setText: its update/layout events may be synchronous.
        label->setProperty(kRichTextCache, readable);
        label->setProperty(kRichTextFormatCache, static_cast<int>(label->textFormat()));
        if (readable != current) {
            label->setText(readable);
        }
    } else if (QTextEdit* edit = qobject_cast<QTextEdit*>(w)) {
        QTextDocument* document = edit->document();
        if (!document->property(kRichDocumentConnected).toBool()) {
            document->setProperty(kRichDocumentConnected, true);
            QObject::connect(document, &QTextDocument::contentsChanged, edit, [edit] {
                floorRichDocument(edit);
            });
        }
        floorRichDocument(edit);
    }
}

constexpr auto kFontOnlyOriginalQss = "_nereusWorkspaceFontOnlyOriginalQss";
constexpr auto kFontOnlyThemedQss = "_nereusWorkspaceFontOnlyThemedQss";

void themeTypographyOnly(QWidget* w)
{
    floorWidgetFont(w);
    floorRichText(w);
    const QString current = w->styleSheet();
    const QVariant themed = w->property(kFontOnlyThemedQss);
    if (!themed.isValid() || themed.toString() != current) {
        w->setProperty(kFontOnlyOriginalQss, current);
        QssTranslator::Options options;
        setFontFloorOptions(options, w);
        options.translateColours = false;
        const QString readable = QssTranslator::translate(current, options).qss;
        // Record before setStyleSheet's synchronous StyleChange. Nothing
        // from the color/component pass is applied to a skipped surface.
        w->setProperty(kFontOnlyThemedQss, readable);
        if (readable != current) {
            w->setStyleSheet(readable);
        }
    }
    floorWidgetFont(w);
}

void themeTypographyRecursive(QWidget* w)
{
    themeTypographyOnly(w);
    const QObjectList kids = w->children();
    for (QObject* child : kids) {
        if (child->isWidgetType()) {
            themeTypographyRecursive(static_cast<QWidget*>(child));
        }
    }
}

void themeOne(QWidget* w)
{
    floorWidgetFont(w);
    floorRichText(w);
    const QString current = w->styleSheet();
    const QVariant themed = w->property(kThemedQssProperty);
    const QVariant stored = w->property(kOriginalQssProperty);

    QString original;
    bool fresh = false;
    if (!stored.isValid() || !themed.isValid() || current != themed.toString()) {
        // First visit, or the widget set a new stylesheet since the
        // last pass: that stylesheet is the new original.
        original = current;
        w->setProperty(kOriginalQssProperty, original);
        fresh = true;
    } else {
        original = stored.toString();
    }

    // A box no sheet styles the indicator of gets JJ's indicator rules
    // with what keeps its size, measured once per original sheet; so do a
    // slider no sheet styles (the decided slider), a checkable group
    // box (the decided indicator) and a progress bar no sheet styles (the
    // selection glass as its filled part).
    const bool toggleBlock = isToggle(w) && !indicatorStyled(w, original);
    const bool sliderNeedsBlock = !toggleBlock && isUnstyledSlider(w, original);
    const bool groupNeedsBlock = !toggleBlock && !sliderNeedsBlock && isUnstyledCheckableGroup(w, original);
    const bool progressNeedsBlock =
        !toggleBlock && !sliderNeedsBlock && !groupNeedsBlock && isUnstyledProgressBar(w, original);
    const bool needsBlock = toggleBlock || sliderNeedsBlock || groupNeedsBlock || progressNeedsBlock;
    QString block;
    if (needsBlock && !fresh) {
        block = w->property(kIndicatorBlockProperty).toString();
    }

    const QString fit = sliderFit(w);

    if (original.isEmpty() && !needsBlock && fit.isEmpty()) {
        w->setProperty(kThemedQssProperty, original);
        return;
    }

    QssTranslator::Options options;
    options.resolveAmbiguousByContext = true;
    setFontFloorOptions(options, w);
    const QssTranslator::Result r = QssTranslator::translate(original, options);
    if (fresh) {
        // A sheet set by a call-site role (applied when the switch is
        // on) already holds palette values; those are not legacy
        // literals and are left out of the report.
        const QSet<QString>& ours = workspaceValues();
        for (const QString& lit : r.unknownLiterals) {
            if (!ours.contains(lit)) {
                ++unknownCounts()[lit];
            }
        }
    }
    QString look = Workspace::Components::componentize(r.qss, w);
    if (needsBlock) {
        // A brace-less sheet is a "*" rule; written as one, so rules can
        // follow it.
        if (!look.trimmed().isEmpty() && !look.contains(QLatin1Char('{'))) {
            look = QStringLiteral("* {%1}\n").arg(look);
        }
        if (block.isEmpty()) {
            block = toggleBlock        ? indicatorBlock(w, look, current)
                  : sliderNeedsBlock   ? sliderBlock(w, look, current)
                  : progressNeedsBlock ? progressBlock(w, look, current)
                                       : groupBoxBlock(w);
            w->setProperty(kIndicatorBlockProperty, block);
        }
        look += block;
    }
    if (!fit.isEmpty()) {
        if (!look.trimmed().isEmpty() && !look.contains(QLatin1Char('{'))) {
            look = QStringLiteral("* {%1}\n").arg(look);
        }
        look += fit;
    }
    // Recorded first: setStyleSheet() sends StyleChange, and the live
    // pass must see this sheet as already themed.
    w->setProperty(kThemedQssProperty, look);
    if (look != current) {
        w->setStyleSheet(look);
    }
    floorWidgetFont(w);
}

void themeRecursive(QWidget* w)
{
    if (skipped(w)) {
        themeTypographyRecursive(w);
        return;
    }
    themeOne(w);
    const QObjectList kids = w->children();
    for (QObject* o : kids) {
        if (o->isWidgetType()) {
            themeRecursive(static_cast<QWidget*>(o));
        }
    }
}

constexpr auto kShowFilterName = "workspacePreviewShowFilter";

class ShowFilter : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Show && watched->isWidgetType()) {
            themeSubtree(static_cast<QWidget*>(watched));
        }
        return QObject::eventFilter(watched, event);
    }
};

constexpr auto kLiveFilterName = "workspacePreviewLiveTheming";

// The live pass for one widget, from a Show or StyleChange event. Its
// children get their own Show events, so it does not recurse.
void themeLive(QWidget* w)
{
    if (skippedInWindow(w)) {
        themeTypographyOnly(w);
        return;
    }
    floorWidgetFont(w);
    floorRichText(w);
    const QVariant themed = w->property(kThemedQssProperty);
    if (themed.isValid() && themed.toString() == w->styleSheet()) {
        return;
    }
    themeOne(w);
}

class LiveThemingFilter : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        // Every event of the application passes here: anything but Show
        // StyleChange, FontChange or Paint leaves after one comparison.
        const QEvent::Type type = event->type();
        if (type != QEvent::Show && type != QEvent::StyleChange
            && type != QEvent::FontChange && type != QEvent::Paint) {
            return false;
        }
        if (!watched->isWidgetType() || !enabled()) {
            return false;
        }
        auto* w = static_cast<QWidget*>(watched);
        if (type == QEvent::Paint) {
            // QLabel has no textChanged signal. Compare its cached text at
            // the paint boundary; only changed HTML runs the token pass.
            if (qobject_cast<QLabel*>(w) != nullptr) {
                floorRichText(w);
            }
            return false;
        }
        if (type == QEvent::FontChange) {
            floorWidgetFont(w);
            return false;
        }
        // A new stylesheet on a hidden widget waits for its next Show,
        // when its size (and so its radius) is known.
        if (type == QEvent::StyleChange && !w->isVisible()) {
            return false;
        }
        themeLive(w);
        return false;
    }
};

} // namespace

int readableTextPixelSize(int pixels)
{
    return enabled() ? std::max(pixels, kMinimumTextPixels) : pixels;
}

QFont readableFont(const QFont& font)
{
    return readableFont(font, nullptr);
}

QFont readableFont(const QFont& font, const QPaintDevice* device)
{
    return readableFontAtDpi(font, QFontMetricsF(font, device).fontDpi());
}

bool enabled()
{
    std::optional<bool>& state = switchState();
    if (!state.has_value()) {
        state = enabledFromEnvironmentValue(qEnvironmentVariable("NEREUS_WORKSPACE_PREVIEW"));
    }
    return *state;
}

bool enabledFromEnvironmentValue(const QString& value)
{
    // On by default: only "0" turns the look off. Unset and empty (both
    // an empty string here), "1" and anything else are on.
    return value != QLatin1String("0");
}

void setEnabledForTest(bool on)
{
    switchState() = on;
}

void themeSubtree(QWidget* root)
{
    if (root == nullptr || !enabled()) {
        return;
    }
    themeRecursive(root);
}

void themeOnShow(QWidget* dialog)
{
    if (dialog == nullptr || !enabled()) {
        return;
    }
    // The filter is a child of the dialog, so it goes with it. One per
    // dialog: a second call (a dialog rebuilt or shown again) finds it.
    if (dialog->findChild<QObject*>(QString::fromLatin1(kShowFilterName),
                                    Qt::FindDirectChildrenOnly) != nullptr) {
        return;
    }
    auto* filter = new ShowFilter(dialog);
    filter->setObjectName(QString::fromLatin1(kShowFilterName));
    dialog->installEventFilter(filter);
}

void installLiveTheming(QApplication& app)
{
    if (!enabled()) {
        return;
    }
    // What the look changes in the platform style's own drawing.
    Workspace::LookStyle::install(app);
    if (app.findChild<QObject*>(QString::fromLatin1(kLiveFilterName),
                                Qt::FindDirectChildrenOnly) != nullptr) {
        return;
    }
    auto* filter = new LiveThemingFilter(&app);
    filter->setObjectName(QString::fromLatin1(kLiveFilterName));
    app.installEventFilter(filter);
}

QString lookQss(const QString& original, const QWidget* owner)
{
    if (!enabled()) {
        return original;
    }
    QssTranslator::Options options;
    options.resolveAmbiguousByContext = true;
    setFontFloorOptions(options, owner);
    return Workspace::Components::componentize(
        QssTranslator::translate(original, options).qss, owner);
}

QColor roleColour(const QColor& legacy, const char* role)
{
    if (!enabled()) {
        return legacy;
    }
    if (!legacy.isValid() || role == nullptr) {
        return legacy;
    }
    QColor c = Workspace::WorkspacePalette::glassAndDepth().color(QString::fromLatin1(role));
    if (!c.isValid()) {
        return legacy;
    }
    c.setAlpha(legacy.alpha());
    return c;
}

QString roleColourName(const char* legacy, const char* role)
{
    if (!enabled()) {
        return QString::fromLatin1(legacy);
    }
    const QColor c = roleColour(QColor::fromString(QLatin1StringView(legacy)), role);
    return c.name(c.alpha() < 255 ? QColor::HexArgb : QColor::HexRgb);
}

namespace {

// The indicator tones Longpath keeps for a live state, a reading and
// transmit (WorkspacePalette.h kLiveGreen, kAmberText, kTxRed).
const char* indicatorRole(const char* role)
{
    const QLatin1StringView r(role);
    if (r == QLatin1StringView("ok")) {
        return "live";
    }
    if (r == QLatin1StringView("warn")) {
        return "measured";
    }
    if (r == QLatin1StringView("danger")) {
        return "tx";
    }
    return role;
}

} // namespace

QColor statusColour(const QColor& legacy, const char* role)
{
    if (!enabled() || role == nullptr) {
        return legacy;
    }
    return roleColour(legacy, indicatorRole(role));
}

QString statusColourName(const char* legacy, const char* role)
{
    if (!enabled() || role == nullptr) {
        return QString::fromLatin1(legacy);
    }
    const QColor c = statusColour(QColor::fromString(QLatin1StringView(legacy)), role);
    return c.name(c.alpha() < 255 ? QColor::HexArgb : QColor::HexRgb);
}

QColor dataBackground(const QColor& legacy, const char* role, const QObject* surface)
{
    if (!enabled()) {
        return legacy;
    }
    // A meter item: start from the widget that owns it.
    const QObject* o = surface;
    while (o != nullptr && !o->isWidgetType()) {
        o = o->parent();
    }
    // The nearest container around the surface decides: its user colour
    // when it has one, otherwise its role.
    for (auto* w = static_cast<const QWidget*>(o); w != nullptr; w = w->parentWidget()) {
        const QVariant v = w->property(kUserBackgroundProperty);
        if (!v.isValid()) {
            continue;
        }
        QColor user = v.value<QColor>();
        if (!user.isValid() || !legacy.isValid()) {
            break;
        }
        if (legacy.alpha() < 255) {
            user.setAlpha(legacy.alpha());
        }
        return user;
    }
    return roleColour(legacy, role);
}

QHash<QString, int> unknownLiteralCounts()
{
    return unknownCounts();
}

void resetUnknownLiteralCounts()
{
    unknownCounts().clear();
}

void passInputThrough(QWidget* widget)
{
    if (widget == nullptr) {
        return;
    }
    widget->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    widget->setWindowFlag(Qt::WindowTransparentForInput, true);
    if (QWindow* window = widget->windowHandle()) {
        window->setFlag(Qt::WindowTransparentForInput, true);
    }
}

} // namespace NereusSDR::WorkspacePreview
