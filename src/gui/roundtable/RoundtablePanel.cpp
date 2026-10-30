// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/roundtable/RoundtablePanel.cpp  (NereusSDR)
// =================================================================
//
// See RoundtablePanel.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-30: Approved frequency-first Roundtable desktop surfaces.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 13).
//   2026-10-24: Roundtable plan Task 15: while nothing about the station
//               is sent, its own reading is the "You" row, marked NOT
//               SENT (setOwnReading); your own turn is found from the
//               talkers and your row, not from `mine`
//               (roundtableTurnIsYours). J.J. Boyd (KG4VCF), AI-assisted
//               via Claude Code.
//   2026-10-24: Roundtable plan Task 15: the panel fits the real applet
//               column (260 px and up) as well as the design's 340 px:
//               the readings columns fit the room's width (the reading
//               bar takes the slack, callsigns elide past 6 characters
//               with the full one in a tooltip), the count, Back to my
//               slice, the name caption and the name box move to lines of
//               their own, two talkers at once take a line each, NOT
//               SENT takes your row's km and Offset cells, and prose
//               wraps. The fit leaves room for a scroll bar (roomWidth).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25: every color is the workspace look's role (Style:: and
//               WorkspacePreview::roleColourName / statusColourName), with
//               no private copy of the palette: "you" is the look's accent,
//               one TX red, the reading bars and the slice letter badge in
//               the slice's own color with readable ink. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-25: your own NOT SENT reading is not present when the slice
//               has none (a dBm outside the wire's range, -400 after a
//               clear), so it shows "none" like every other row, not S0
//               (JJ's answer of 2026-10-25). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-25: the header's frequency is in the workspace's monospace
//               families (Workspace::kMonoQssFamilies), and in the
//               narrowest column, after the count, the mode goes under the
//               frequency beside the count (fitCount), so a 10-character
//               frequency fits 260 px with Linux and Mac fonts. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25: a rebuilt section's body shows as soon as it is filled
//               (showFreshBodies), not on the show its layout queues, so no
//               layout pass sees the section empty and pulls a view that is
//               not at the newest message up by its height (the readings,
//               rebuilt every second). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-26: the button that opens Roundtable's own window shows a
//               small window (OwnWindowButton) with the tooltip and
//               accessible name "Open in its own window", so it is not
//               taken for the container header's Pop Out arrow beside it.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: the panel menu's action and the menu button's tooltip
//               say "Open in its own window" too, never "Pop out".
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26: your own reading's range is RoundtableWire::kMinDbm and
//               kMaxDbm, shared with the Core's client. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "gui/roundtable/RoundtablePanel.h"
#include "gui/roundtable/RoundtableTagLayout.h"

#include "core/AppSettings.h"
#include "core/ControlRanges.h"
#include "core/roundtable/RoundtableWire.h"
#include "gui/StyleConstants.h"
#include "gui/styles/WorkspaceStyle.h"
#include "gui/styles/WorkspacePreview.h"
#include "models/RoundtableModel.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFocusEvent>
#include <QFontDatabase>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyle>
#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QTimeZone>
#include <QToolButton>
#include <QUuid>
#include <QFontMetrics>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <memory>

namespace NereusSDR {

namespace {

bool historyConnected(const RoundtableModel* model)
{
    return model && (model->linkState() == RoundtableModel::LinkState::Live
                     || model->linkState() == RoundtableModel::LinkState::Hidden);
}

// The placement mockup's colors (placement.html :root) and Roundtable's
// state colors (design 11, U7, U8), each the workspace look's role: with the
// look off, the classic Style:: value the role stands for
// (WorkspacePreview::roleColourName). The state colors are status roles:
// talking is live, unknown is measured, a double is the transmit red. "You"
// is the look's accent everywhere. The mockup's 9 and 10 px text is 11 px
// here (U12: never under 11 px).
QString groundColor() { return WorkspacePreview::roleColourName(Style::kPanelBg, "panel"); }
QString ruleColor() { return WorkspacePreview::roleColourName(Style::kBorderSubtle, "border-subtle"); }
QString textColor() { return WorkspacePreview::roleColourName(Style::kTextPrimary, "text"); }
QString dimColor() { return WorkspacePreview::roleColourName(Style::kTextSecondary, "text-secondary"); }
QString dim2Color() { return WorkspacePreview::roleColourName(Style::kTextScale, "text-scale"); }
QString accentColor() { return WorkspacePreview::roleColourName(Style::kAccent, "accent"); }
QString youColor() { return accentColor(); }
QString talkingColor() { return WorkspacePreview::statusColourName(Style::kGreenText, "ok"); }
QString unknownColor() { return WorkspacePreview::statusColourName(Style::kAmberText, "warn"); }
QString doubledColor() { return WorkspacePreview::statusColourName(Style::kRedBorder, "danger"); }
// Design 11.1: a stale row turns grey at about 5:1, not faded.
QString staleColor() { return WorkspacePreview::roleColourName(Style::kLabelMid, "label-mid"); }
// Away readings, a step under the text.
QString awayColor() { return WorkspacePreview::roleColourName(Style::kTextTertiary, "text-tertiary"); }
// Text on a filled selection or under the pointer.
QString brightColor() { return WorkspacePreview::roleColourName(Style::kBlueText, "sel-text"); }
// The one TX red: the look's transmit status, its text the danger text.
QString txColor() { return WorkspacePreview::statusColourName(Style::kRedBg, "danger"); }
QString txTextColor() { return WorkspacePreview::roleColourName(Style::kRedText, "danger-text"); }
// A focused row's wash.
QString focusColor() { return WorkspacePreview::roleColourName(Style::kButtonAltHover, "badge-info-bg"); }
QString role(const char* legacy, const char* name) { return WorkspacePreview::roleColourName(legacy, name); }

// Your reading row's tint: the look's solid info tint, the classic look's
// translucent accent.
QString yourRowColor()
{
    return WorkspacePreview::enabled() ? role(Style::kAccent, "badge-info-bg")
                                       : QStringLiteral("rgba(0, 180, 216, 20)");
}

// The WCAG relative luminance of a color (WCAG 2.x section 1.4.3).
double luminance(const QColor& c)
{
    const auto channel = [](double v) {
        return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(c.redF()) + 0.7152 * channel(c.greenF()) + 0.0722 * channel(c.blueF());
}

double contrast(const QColor& a, const QColor& b)
{
    const double la = luminance(a);
    const double lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

// The look's dark or light ink, whichever reads better on `fill`.
QString inkOn(const QColor& fill)
{
    const QString dark = role(Style::kAppBg, "app-bg");
    const QString light = brightColor();
    return contrast(QColor::fromString(dark), fill) >= contrast(QColor::fromString(light), fill) ? dark : light;
}

// The slice letter badge in the slice's own color, the color the VFO flag's
// letter badge has (VfoWidget::sliceColor, ControlRanges::kSliceColours).
QString sliceBadgeStyle(int sliceId)
{
    const int count = static_cast<int>(ControlRanges::kSliceColours.size());
    const QColor fill(static_cast<QRgb>(ControlRanges::sliceColour(std::max(sliceId, 0) % count)));
    return QStringLiteral(" color: %1; background: %2; padding: 0 4px; border-radius: 2px;")
        .arg(inkOn(fill), fill.name());
}

constexpr int kSmallPx = 11;
constexpr int kBodyPx = 12;
// The readings columns (mockup .rd, with the Age column the design adds).
constexpr int kCallWidth = 84;
constexpr int kKmWidth = 40;
constexpr int kOffsetWidth = 40;
constexpr int kValueWidth = 46;
constexpr int kAgeWidth = 40;
// The reading bar's least width, the slack the columns leave above it.
constexpr int kReadingBarMinWidth = 24;
// In the narrowest column the bar gives way before a value does, down to
// this.
constexpr int kReadingBarNarrowWidth = 10;
// The rooms the empty state offers to read.
constexpr int kEmptyRooms = 8;
// Name chips beside the callsign box (design 8.6 names).
constexpr int kNameChips = 3;
// A message box two lines tall.
constexpr int kBoxHeight = 46;
// S9 is -73 dBm, 6 dB an S-unit (the mockup's sText).
constexpr double kS9Dbm = -73.0;
constexpr double kDbPerSUnit = 6.0;

QString buttonStyle()
{
    return QStringLiteral(
               "QPushButton { background: %1; border: 1px solid %2; border-radius: 3px;"
               "  color: %3; font-size: 11px; padding: 2px 8px; }"
               "QPushButton:hover { border-color: %4; color: %5; }"
               "QPushButton:disabled { color: %6; border-color: %7; }")
        .arg(role(Style::kButtonBg, "button"), role(Style::kBorder, "border"), textColor(), accentColor(),
             brightColor(), role(Style::kDisabledText, "disabled-text"),
             role(Style::kDisabledBorder, "disabled-border"));
}
QString primaryStyle()
{
    return QStringLiteral(
               "QPushButton { background: %1; border: 1px solid %2; border-radius: 3px;"
               "  color: %3; font-size: 11px; font-weight: bold; padding: 2px 10px; }"
               "QPushButton:hover { background: %4; }"
               "QPushButton:disabled { background: %5; border-color: %6; color: %7; }")
        .arg(role(Style::kBlueBg, "selected-top"), accentColor(), brightColor(),
             role(Style::kBlueHover, "selected-border"), role(Style::kDisabledBg, "disabled-bg"),
             role(Style::kDisabledBorder, "disabled-border"), role(Style::kDisabledText, "disabled-text"));
}
QString linkStyle()
{
    return QStringLiteral(
               "QPushButton { background: transparent; border: none; color: %1; font-size: 11px;"
               "  padding: 0 2px; }"
               "QPushButton:hover { color: %2; }")
        .arg(accentColor(), brightColor());
}
QString pillStyle()
{
    return QStringLiteral(
               "QPushButton { background: %1; border: 1px solid %2; border-radius: 9px;"
               "  color: %3; font-size: 11px; font-weight: bold; padding: 1px 10px; }"
               "QPushButton:hover { background: %4; }")
        .arg(role(Style::kBlueBg, "selected-bottom"), accentColor(), brightColor(),
             role(Style::kBlueHover, "selected-top"));
}
QString mentionPillStyle()
{
    const QString fill = youColor();
    return QStringLiteral(
               "QPushButton { background: %1; border: 1px solid %1; border-radius: 9px;"
               "  color: %2; font-size: 11px; font-weight: bold; padding: 1px 10px; }")
        .arg(fill, inkOn(QColor::fromString(fill)));
}
QString iconStyle()
{
    return QStringLiteral(
               "QToolButton { background: transparent; border: 1px solid transparent; border-radius: 3px;"
               "  color: %1; font-size: 12px; padding: 0 4px; }"
               "QToolButton:hover { color: %2; border-color: %3; }"
               "QToolButton::menu-indicator { image: none; width: 0; }")
        .arg(dimColor(), brightColor(), role(Style::kBorder, "border"));
}

// The button that opens Roundtable's own OS window. It draws a small window
// (an outline with a solid title bar) where the other header buttons draw a
// glyph, so it is not taken for the container header's Pop Out arrow, which
// makes a pop-out container instead. The app has no window icon or glyph to
// reuse, and no font is sure to carry one. It keeps the arrow as its text
// only so it measures exactly as the arrow button did; the text is never
// painted, and the accessible name says what it does. Colors follow
// iconStyle(): dim, bright under the pointer.
class OwnWindowButton : public QToolButton
{
public:
    explicit OwnWindowButton(QWidget* parent) : QToolButton(parent)
    {
        setText(QString(QChar(0x2197)));
        setToolTip(QStringLiteral("Open in its own window"));
        setAccessibleName(QStringLiteral("Open in its own window"));
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QStylePainter painter(this);
        QStyleOptionToolButton opt;
        initStyleOption(&opt);
        opt.text.clear();
        painter.drawComplexControl(QStyle::CC_ToolButton, opt);

        // The window: as wide as the arrow's cap height and a little less
        // tall, centered in the button, on whole pixels.
        const QFontMetrics fm(font());
        const int w = std::max(8, fm.capHeight() + 1);
        const int h = std::max(6, w - 2);
        const int x = (width() - w) / 2;
        const int y = (height() - h) / 2;
        const bool hot = (opt.state & QStyle::State_MouseOver) != 0;
        const QColor ink(hot ? brightColor() : dimColor());
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setPen(Qt::NoPen);
        painter.setBrush(ink);
        painter.drawRect(x, y, w, 2);              // title bar
        painter.drawRect(x, y + h - 1, w, 1);      // bottom
        painter.drawRect(x, y, 1, h);              // left
        painter.drawRect(x + w - 1, y, 1, h);      // right
    }
};

QToolButton* makeOwnWindowButton(QWidget* parent)
{
    auto* button = new OwnWindowButton(parent);
    button->setObjectName(QStringLiteral("roundtablePopBtn"));
    button->setStyleSheet(iconStyle());
    button->setFocusPolicy(Qt::ClickFocus);
    return button;
}

QString inputStyle()
{
    return QStringLiteral(
               "QLineEdit, QPlainTextEdit { background: %1; border: 1px solid %2;"
               "  border-radius: 3px; color: %3; font-size: 12px; padding: 2px 5px;"
               "  placeholder-text-color: %4; }"
               "QLineEdit:focus, QPlainTextEdit:focus { border-color: %5; }"
               "QLineEdit:disabled, QPlainTextEdit:disabled { color: %6; border-color: %7; }")
        .arg(role(Style::kInsetBg, "inset"), role(Style::kBorder, "border-muted"), textColor(),
             role(Style::kTextTertiary, "text-tertiary"), accentColor(),
             role(Style::kDisabledText, "disabled-text"), role(Style::kDisabledBorder, "disabled-border"));
}

// The monospace fallback chain the workspace's own instruments use, so a
// machine without the first family still gets a fixed-pitch one (the
// offscreen platform's system FixedFont on the Mac is proportional).
QString mono()
{
    return QLatin1String(Workspace::kMonoQssFamilies);
}

QString labelStyle(const QString& color, int px, bool bold, bool isMono)
{
    QString style = QStringLiteral("color: %1; font-size: %2px; background: transparent;")
                        .arg(color)
                        .arg(px);
    if (bold) {
        style += QStringLiteral(" font-weight: bold;");
    }
    if (isMono) {
        style += QStringLiteral(" font-family: %1;").arg(mono());
    }
    return style;
}

QLabel* makeLabel(QWidget* parent, const QString& text, const QString& color, int px,
                  bool bold = false, bool isMono = false)
{
    auto* label = new QLabel(text, parent);
    label->setStyleSheet(labelStyle(color, px, bold, isMono));
    return label;
}

// The AWAY and NOT SENT tags beside a reading's call.
QString readingTagStyle()
{
    return labelStyle(dimColor(), kSmallPx, true, false)
         + QStringLiteral(" border: 1px solid %1; border-radius: 2px; padding: 0 2px;")
               .arg(role(Style::kBorder, "border-muted"));
}

QLabel* capLabel(QWidget* parent, const QString& text)
{
    QLabel* label = makeLabel(parent, text, dimColor(), kSmallPx, true);
    QFont font = label->font();
    font.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    label->setFont(font);
    return label;
}

QPushButton* makeButton(QWidget* parent, const QString& text, const QString& style)
{
    auto* button = new QPushButton(text, parent);
    button->setStyleSheet(style);
    button->setCursor(Qt::PointingHandCursor);
    button->setFocusPolicy(Qt::ClickFocus);
    return button;
}

// The one body a section holds, replaced whole on each rebuild.
QWidget* freshBody(QWidget* section)
{
    auto* layout = static_cast<QVBoxLayout*>(section->layout());
    if (QLayoutItem* raw = layout->takeAt(0)) {
        std::unique_ptr<QLayoutItem> item(raw);
        if (QWidget* old = item->widget()) {
            old->hide();
            old->setParent(nullptr);
            old->deleteLater();
        }
    }
    auto* body = new QWidget(section);
    layout->addWidget(body);
    return body;
}

// The body freshBody() put in each section shows now that it is filled.
// Added to a visible section, the layout only queues its show
// (QLayout::addChildWidget); a layout pass that ran first laid the section
// out empty, the scroll range shrank by its height and a view that was not
// at the newest message stayed that much higher when the body showed. The
// rule is the queued show's: a body hidden on purpose stays hidden.
void showFreshBodies(std::initializer_list<QWidget*> sections)
{
    for (QWidget* section : sections) {
        QLayoutItem* item = section->layout() != nullptr ? section->layout()->itemAt(0) : nullptr;
        QWidget* body = item != nullptr ? item->widget() : nullptr;
        if (body != nullptr && body->isHidden() && !body->testAttribute(Qt::WA_WState_ExplicitShowHide)) {
            body->show();
        }
    }
}

QWidget* makeSection(QWidget* parent, const QString& name)
{
    auto* section = new QWidget(parent);
    section->setObjectName(name);
    auto* layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    return section;
}

QString rule()
{
    return QStringLiteral("border-bottom: 1px solid %1;").arg(ruleColor());
}

QString compact(const QJsonValue& value)
{
    if (value.isArray()) {
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    }
    return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
}

// 7185000 -> "7.185.000", the room header's frequency (mockup fmtF).
QString headerFrequency(qint64 hz)
{
    const QString digits = QString::number(hz).rightJustified(7, QLatin1Char('0'));
    return digits.left(digits.size() - 6) + QLatin1Char('.') + digits.mid(digits.size() - 6, 3)
         + QLatin1Char('.') + digits.right(3);
}

// 7249000 -> "7.249", the read bar's (design 11.1).
QString shortFrequency(qint64 hz)
{
    return QString::number(static_cast<double>(hz) / 1.0e6, 'f', 3);
}

QString modeLabel(const QString& family)
{
    if (family == QLatin1String("fdvl") || family == QLatin1String("fdvu")) {
        return QStringLiteral("FreeDV");
    }
    return family.toUpper();
}

// The mockup's sText: S1 to S9, then S9 +N.
QString sText(double dbm)
{
    const int over = static_cast<int>(std::lround(dbm - kS9Dbm));
    if (over < 1) {
        const int units = static_cast<int>(std::lround(9.0 - (kS9Dbm - dbm) / kDbPerSUnit));
        return QStringLiteral("S%1").arg(std::clamp(units, 0, 9));
    }
    return QStringLiteral("S9 +%1").arg(over);
}

double sFraction(double dbm)
{
    return std::clamp((dbm + 127.0) / 100.0, 0.02, 1.0);
}

QString offsetText(qint64 hz)
{
    if (std::llabs(hz) < 50) {
        return QStringLiteral("on");
    }
    const qint64 rounded = std::llabs((hz + (hz > 0 ? 50 : -50)) / 100 * 100);
    return QStringLiteral("%1%2 Hz").arg(hz > 0 ? QStringLiteral("+") : QStringLiteral("-")).arg(rounded);
}

QString ageText(qint64 ms)
{
    const qint64 seconds = std::max<qint64>(0, ms / 1000);
    if (seconds < 60) {
        return QStringLiteral("%1 s").arg(seconds);
    }
    if (seconds < 3600) {
        return QStringLiteral("%1 min").arg(seconds / 60);
    }
    return QStringLiteral("%1 h").arg(seconds / 3600);
}

QString clockText(qint64 ms)
{
    const qint64 seconds = std::max<qint64>(0, ms / 1000);
    return QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

QString utcText(qint64 unixMs)
{
    return QDateTime::fromMSecsSinceEpoch(unixMs, Qt::UTC).toString(QStringLiteral("HH:mm"));
}

QString personKey(const QJsonObject& who)
{
    const QString ref = who.value(QStringLiteral("ref")).toString();
    const QString source = who.value(QStringLiteral("sourceRoom")).toString();
    return source.isEmpty() ? ref : source + QLatin1Char('/') + ref;
}

QString withNth(const QJsonObject& who)
{
    const QString callsign = who.value(QStringLiteral("callsign")).toString();
    const int nth = who.value(QStringLiteral("nth")).toInt();
    return nth > 1 ? QStringLiteral("%1 (%2)").arg(callsign).arg(nth) : callsign;
}

QString roomFamily(const QString& roomId)
{
    const qsizetype dash = roomId.indexOf(QLatin1Char('-'));
    return dash > 0 ? roomId.left(dash).section(QLatin1Char(':'), -1) : QString();
}

qint64 roomHz(const QString& roomId)
{
    const qsizetype dash = roomId.indexOf(QLatin1Char('-'));
    return dash > 0 ? roomId.mid(dash + 1).toLongLong() : 0;
}

// Wire spec 5.3: bold, italic and http/https links shown with their full
// address; everything else as plain text.
QString formatted(const QString& text)
{
    QString html = text.toHtmlEscaped();
    static const QRegularExpression link(QStringLiteral("\\b(https?://[^\\s<]+)"));
    static const QRegularExpression bold(QStringLiteral("\\*\\*([^*\\n]+)\\*\\*"));
    static const QRegularExpression italic(QStringLiteral("\\*([^*\\n]+)\\*"));
    html.replace(bold, QStringLiteral("<b>\\1</b>"));
    html.replace(italic, QStringLiteral("<i>\\1</i>"));
    html.replace(link, QStringLiteral("<a href=\"\\1\" style=\"color: %1;\">\\1</a>").arg(accentColor()));
    html.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    return html;
}

// A reading's bar (mockup .rd .b): green, striped red for a double, grey
// once stale.
class ReadingBar : public QWidget {
public:
    ReadingBar(double fraction, bool doubled, bool stale, QWidget* parent)
        : QWidget(parent), m_fraction(fraction), m_doubled(doubled), m_stale(stale)
    {
        setFixedHeight(8);
        setMinimumWidth(kReadingBarMinWidth);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const QRect frame = rect().adjusted(0, 0, -1, -1);
        p.fillRect(frame, WorkspacePreview::roleColour(QColor(Style::kInsetBg), "inset"));
        p.setPen(WorkspacePreview::roleColour(QColor(Style::kBorder), "border"));
        p.drawRect(frame);
        const int width = static_cast<int>(std::lround(m_fraction * (frame.width() - 1)));
        const QRect fill(frame.left() + 1, frame.top() + 1, width, frame.height() - 1);
        if (m_stale) {
            p.fillRect(fill, WorkspacePreview::roleColour(QColor(Style::kTextInactive), "text-inactive"));
        } else if (m_doubled) {
            p.fillRect(fill, WorkspacePreview::roleColour(QColor(Style::kRedBg), "badge-tx-bg"));
            p.setPen(QPen(WorkspacePreview::statusColour(QColor(Style::kRedBorder), "danger"), 2));
            p.setClipRect(fill);
            for (int x = fill.left() - fill.height(); x < fill.right(); x += 6) {
                p.drawLine(x, fill.bottom(), x + fill.height(), fill.top());
            }
        } else {
            QLinearGradient gradient(fill.topLeft(), fill.topRight());
            gradient.setColorAt(0.0, WorkspacePreview::roleColour(QColor(Style::kGreenBorder), "ok-border"));
            gradient.setColorAt(1.0, WorkspacePreview::statusColour(QColor(Style::kGreenText), "ok"));
            p.fillRect(fill, gradient);
        }
    }

private:
    double m_fraction;
    bool m_doubled;
    bool m_stale;
};

// A one-line label that fits a narrow column by eliding its end, never by
// clipping: it keeps its first `keep` characters (a callsign stays whole
// to 6), and while elided its full text is the tooltip. Each line of a
// two-line text elides on its own.
class ElideLabel : public QLabel {
public:
    ElideLabel(const QString& text, int keep, QWidget* parent)
        : QLabel(text, parent), m_keep(keep)
    {
        setMinimumWidth(0);
    }

    QSize minimumSizeHint() const override
    {
        QSize size = QLabel::minimumSizeHint();
        const QFontMetrics metrics(font());
        int widest = 0;
        for (const QString& line : text().split(QLatin1Char('\n'))) {
            const QString kept = line.size() > m_keep ? line.left(m_keep) + QChar(0x2026) : line;
            widest = std::max(widest, metrics.horizontalAdvance(kept));
        }
        const QMargins margins = contentsMargins();
        size.setWidth(widest + margins.left() + margins.right() + 2 * margin() + 1);
        return size;
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        const QRect area = contentsRect().adjusted(margin(), margin(), -margin(), -margin());
        const QFontMetrics metrics(font());
        QStringList lines = text().split(QLatin1Char('\n'));
        bool elided = false;
        for (QString& line : lines) {
            const QString shown = metrics.elidedText(line, Qt::ElideRight, area.width());
            elided = elided || shown != line;
            line = shown;
        }
        style()->drawItemText(&painter, area, static_cast<int>(alignment()), palette(), isEnabled(),
                              lines.join(QLatin1Char('\n')), foregroundRole());
        // The whole text on hover while it does not fit.
        const QString tip = elided ? text() : QString();
        if (m_ownTip != tip && (toolTip().isEmpty() || toolTip() == m_ownTip)) {
            m_ownTip = tip;
            setToolTip(tip);
        }
    }

private:
    int m_keep;
    QString m_ownTip;
};

// Callsigns stay whole to 6 characters before they elide.
constexpr int kCallsignKeep = 6;
// Other one-line text keeps a word's worth.
constexpr int kTextKeep = 4;

ElideLabel* makeElided(QWidget* parent, const QString& text, const QString& color, int px,
                       bool bold = false, bool isMono = false, int keep = kCallsignKeep)
{
    auto* label = new ElideLabel(text, keep, parent);
    label->setStyleSheet(labelStyle(color, px, bold, isMono));
    return label;
}

// Prose that wraps at the room's width instead of widening it.
QLabel* makeWrapped(QWidget* parent, const QString& text, const QString& color, int px,
                    bool bold = false)
{
    QLabel* label = makeLabel(parent, text, color, px, bold);
    label->setWordWrap(true);
    label->setMinimumWidth(0);
    return label;
}

QString messageRowStyle(bool isNew, bool mention)
{
    const QString edge = mention ? youColor() : (isNew ? accentColor() : QStringLiteral("transparent"));
    return QStringLiteral("QFrame#roundtableMessageRow { border: none; border-left: 2px solid %1;"
                          "  border-bottom: 1px solid %2; background: transparent; }"
                          "QFrame#roundtableMessageRow:focus { background: %3; }")
        .arg(edge, ruleColor(), focusColor());
}

QString turnRowStyle()
{
    return QStringLiteral("QFrame#roundtableTurnRow { border: none; border-bottom: 1px solid %1;"
                          "  background: transparent; }"
                          "QFrame#roundtableTurnRow:focus { background: %2; }")
        .arg(ruleColor(), focusColor());
}

} // namespace

RoundtablePanel::RoundtablePanel(RoundtableModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    setObjectName(QStringLiteral("RoundtablePanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral("QWidget#RoundtablePanel { background: %1; }").arg(groundColor()));
    setFocusPolicy(Qt::TabFocus);
    setMinimumWidth(260);
    installEventFilter(this);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* main = new QWidget(this);
    main->setObjectName(QStringLiteral("roundtableMainColumn"));
    auto* mainLayout = new QVBoxLayout(main);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    root->addWidget(main, 1);

    m_scroll = new QScrollArea(main);
    m_scroll->setObjectName(QStringLiteral("roundtableScroll"));
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setFocusPolicy(Qt::NoFocus);
    m_scroll->viewport()->installEventFilter(this);
    m_scroll->setStyleSheet(QStringLiteral("QScrollArea { background: %1; border: none; }")
                                .arg(groundColor()));
    mainLayout->addWidget(m_scroll, 1);

    m_content = new QWidget();
    m_content->setObjectName(QStringLiteral("roundtableContent"));
    m_content->setAttribute(Qt::WA_StyledBackground, true);
    m_content->setStyleSheet(QStringLiteral("QWidget#roundtableContent { background: %1; }")
                                 .arg(groundColor()));
    m_contentLayout = new QVBoxLayout(m_content);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(0);

    // The read bar (design 11.1).
    m_readBar = new QWidget(m_content);
    m_readBar->setObjectName(QStringLiteral("roundtableReadBar"));
    m_readBar->setAttribute(Qt::WA_StyledBackground, true);
    m_readBar->setStyleSheet(QStringLiteral("QWidget#roundtableReadBar { background: %1; %2 }")
                                 .arg(role(Style::kButtonBg, "tile-active-bg"), rule()));
    {
        // "Reading 7.249 · Tune A here · Back to my slice", one line where
        // it fits, else Back to my slice on a second (fitReadBar).
        auto* lines = new QVBoxLayout(m_readBar);
        lines->setContentsMargins(8, 4, 8, 4);
        lines->setSpacing(2);
        auto* row = new QHBoxLayout();
        row->setSpacing(4);
        lines->addLayout(row);
        m_readFirstLine = row;
        m_readSecondLine = new QHBoxLayout();
        m_readSecondLine->setSpacing(4);
        m_readSecondLine->addStretch(1);
        lines->addLayout(m_readSecondLine);
        const auto separator = [this]() {
            QLabel* dot = makeLabel(m_readBar, QStringLiteral("·"), dimColor(), kSmallPx);
            dot->setObjectName(QStringLiteral("roundtableReadSeparator"));
            return dot;
        };
        m_readLabel = makeLabel(m_readBar, QString(), textColor(), kSmallPx, true);
        m_readLabel->setObjectName(QStringLiteral("roundtableReadLabel"));
        row->addWidget(m_readLabel);
        row->addWidget(separator());
        QPushButton* tune = makeButton(m_readBar, QStringLiteral("Tune A here"), primaryStyle());
        tune->setObjectName(QStringLiteral("roundtableTuneHereBtn"));
        tune->setToolTip(QStringLiteral("Tune the slice to this room"));
        connect(tune, &QPushButton::clicked, this, [this]() {
            const QString id = roomId();
            if (!id.isEmpty()) {
                emit tuneRequested(roomHz(id), roomFamily(id));
            }
        });
        row->addWidget(tune);
        m_readBackSeparator = separator();
        row->addWidget(m_readBackSeparator);
        QPushButton* back = makeButton(m_readBar, QStringLiteral("Back to my slice"), linkStyle());
        back->setObjectName(QStringLiteral("roundtableBackBtn"));
        connect(back, &QPushButton::clicked, this, &RoundtablePanel::backToSlice);
        row->addWidget(back);
        m_readBack = back;
        row->addStretch(1);
    }
    m_contentLayout->addWidget(m_readBar);

    m_head = makeSection(m_content, QStringLiteral("roundtableHeader"));
    m_contentLayout->addWidget(m_head);
    m_connection = makeLabel(m_content, QString(), dimColor(), kSmallPx);
    m_connection->setObjectName(QStringLiteral("roundtableConnectionLine"));
    m_connection->setWordWrap(true);
    m_connection->setContentsMargins(8, 3, 8, 3);
    m_contentLayout->addWidget(m_connection);
    m_txLine = makeSection(m_content, QStringLiteral("roundtableTxLine"));
    m_contentLayout->addWidget(m_txLine);
    m_talker = makeSection(m_content, QStringLiteral("roundtableTalker"));
    m_contentLayout->addWidget(m_talker);
    m_readings = makeSection(m_content, QStringLiteral("roundtableReadings"));
    m_contentLayout->addWidget(m_readings);
    m_earlier = makeSection(m_content, QStringLiteral("roundtableEarlier"));
    m_contentLayout->addWidget(m_earlier);
    m_messages = makeSection(m_content, QStringLiteral("roundtableMessages"));
    m_messages->setAttribute(Qt::WA_StyledBackground, true);
    m_messages->setStyleSheet(QStringLiteral("QWidget#roundtableMessages { background: %1; }")
                                  .arg(groundColor()));
    m_contentLayout->addWidget(m_messages);
    m_empty = makeSection(m_content, QStringLiteral("roundtableEmpty"));
    m_contentLayout->addWidget(m_empty);
    m_contentLayout->addStretch(1);
    m_scroll->setWidget(m_content);

    // The right-hand messages column of the widest window.
    m_msgColumn = new QWidget(this);
    m_msgColumn->setObjectName(QStringLiteral("roundtableMessagesColumn"));
    m_msgColumn->setAttribute(Qt::WA_StyledBackground, true);
    m_msgColumn->setStyleSheet(QStringLiteral("QWidget#roundtableMessagesColumn { background: %1;"
                                              " border-left: 1px solid %2; }")
                                   .arg(groundColor(), ruleColor()));
    m_msgColumn->setFixedWidth(kMessagesColumnWidth);
    auto* msgLayout = new QVBoxLayout(m_msgColumn);
    msgLayout->setContentsMargins(0, 0, 0, 0);
    msgLayout->setSpacing(0);
    m_msgScroll = new QScrollArea(m_msgColumn);
    m_msgScroll->setObjectName(QStringLiteral("roundtableMessagesScroll"));
    m_msgScroll->setWidgetResizable(true);
    m_msgScroll->setFrameShape(QFrame::NoFrame);
    m_msgScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_msgScroll->setFocusPolicy(Qt::NoFocus);
    m_msgScroll->setStyleSheet(m_scroll->styleSheet());
    msgLayout->addWidget(m_msgScroll, 1);
    m_msgColumn->hide();
    root->addWidget(m_msgColumn);

    // The footer: pills, the failed send, the box and its hint, pinned.
    m_footer = new QWidget(main);
    m_footer->setObjectName(QStringLiteral("roundtableFooter"));
    m_footer->setAttribute(Qt::WA_StyledBackground, true);
    m_footer->setStyleSheet(QStringLiteral("QWidget#roundtableFooter { background: %1;"
                                           " border-top: 1px solid %2; }")
                                .arg(groundColor(), ruleColor()));
    {
        auto* foot = new QVBoxLayout(m_footer);
        foot->setContentsMargins(8, 5, 8, 5);
        foot->setSpacing(4);
        auto* pills = new QHBoxLayout();
        pills->setSpacing(6);
        m_mentionPill = makeButton(m_footer, QString(), mentionPillStyle());
        m_mentionPill->setObjectName(QStringLiteral("roundtableMentionPill"));
        connect(m_mentionPill, &QPushButton::clicked, this, &RoundtablePanel::jumpToMention);
        pills->addWidget(m_mentionPill);
        m_newPill = makeButton(m_footer, QString(), pillStyle());
        m_newPill->setObjectName(QStringLiteral("roundtableNewPill"));
        connect(m_newPill, &QPushButton::clicked, this, [this]() { scrollMessagesToEnd(); });
        pills->addWidget(m_newPill);
        pills->addStretch(1);
        foot->addLayout(pills);
        m_mentionPill->hide();
        m_newPill->hide();

        m_notSent = new QWidget(m_footer);
        m_notSent->setObjectName(QStringLiteral("roundtableNotSent"));
        auto* notSentRow = new QHBoxLayout(m_notSent);
        notSentRow->setContentsMargins(0, 0, 0, 0);
        notSentRow->setSpacing(4);
        QLabel* notSentLabel = makeLabel(m_notSent, QStringLiteral("Not sent \u00b7"), unknownColor(),
                                         kSmallPx, true);
        notSentLabel->setObjectName(QStringLiteral("roundtableNotSentLabel"));
        notSentRow->addWidget(notSentLabel);
        QPushButton* again = makeButton(m_notSent, QStringLiteral("Try again"), linkStyle());
        again->setObjectName(QStringLiteral("roundtableTryAgainBtn"));
        connect(again, &QPushButton::clicked, this, &RoundtablePanel::send);
        notSentRow->addWidget(again);
        notSentRow->addStretch(1);
        m_notSent->hide();
        foot->addWidget(m_notSent);

        auto* boxRow = new QHBoxLayout();
        boxRow->setSpacing(5);
        m_box = new QPlainTextEdit(m_footer);
        m_box->setObjectName(QStringLiteral("roundtableBox"));
        m_box->setStyleSheet(inputStyle());
        m_box->setFixedHeight(kBoxHeight);
        m_box->setTabChangesFocus(true);
        m_box->installEventFilter(this);
        connect(m_box, &QPlainTextEdit::textChanged, this, [this]() {
            if (!m_settingBox && m_model && !m_shownId.isEmpty()) {
                m_model->setDraft(m_shownDraftKey, m_box->toPlainText());
            }
            updateFooter();
        });
        boxRow->addWidget(m_box, 1);
        m_send = makeButton(m_footer, QStringLiteral("Send"), primaryStyle());
        m_send->setObjectName(QStringLiteral("roundtableSendBtn"));
        connect(m_send, &QPushButton::clicked, this, &RoundtablePanel::send);
        boxRow->addWidget(m_send, 0, Qt::AlignBottom);
        foot->addLayout(boxRow);

        auto* hintRow = new QHBoxLayout();
        hintRow->setSpacing(6);
        m_hint = makeLabel(m_footer, QString(), dim2Color(), kSmallPx);
        m_hint->setObjectName(QStringLiteral("roundtableHint"));
        m_hint->setWordWrap(true);
        hintRow->addWidget(m_hint, 1);
        m_counter = makeLabel(m_footer, QString(), dimColor(), kSmallPx, false, true);
        m_counter->setObjectName(QStringLiteral("roundtableCounter"));
        m_counter->hide();
        hintRow->addWidget(m_counter, 0, Qt::AlignTop);
        foot->addLayout(hintRow);
    }
    mainLayout->addWidget(m_footer);

    for (QScrollArea* area : {m_scroll, m_msgScroll}) {
        connect(area->verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
            checkOnScreen();
        });
        connect(area->verticalScrollBar(), &QScrollBar::rangeChanged, this,
                [this, area](int, int max) {
            // At the newest message the view follows (U11).
            if (area == messagesScroll() && property("roundtableStick").toBool()) {
                area->verticalScrollBar()->setValue(max);
            }
        });
        connect(area->verticalScrollBar(), &QScrollBar::sliderMoved, this, [this, area](int value) {
            if (area == messagesScroll()) {
                setProperty("roundtableStick", value >= area->verticalScrollBar()->maximum() - 4);
            }
        });
        connect(area->verticalScrollBar(), &QScrollBar::actionTriggered, this, [this, area](int) {
            QScrollBar* bar = area->verticalScrollBar();
            if (area == messagesScroll()) {
                setProperty("roundtableStick", bar->sliderPosition() >= bar->maximum() - 4);
            }
        });
    }

    if (m_model) {
        connect(m_model, &RoundtableModel::roomChanged, this, &RoundtablePanel::onRoomChanged);
        connect(m_model, &RoundtableModel::roomsChanged, this, &RoundtablePanel::rebuild);
        connect(m_model, &RoundtableModel::linkChanged, this, [this]() {
            m_failedContext.clear();
            m_historyRequest.clear();
            m_historyRequestedMs = -1;
            m_historyFresh = false;
            rebuild();
        });
        connect(m_model, &RoundtableModel::sliceRoomsChanged, this, &RoundtablePanel::rebuild);
        connect(m_model, &RoundtableModel::mentionsChanged, this, &RoundtablePanel::rebuild);
        connect(m_model, &RoundtableModel::settingChanged, this, &RoundtablePanel::rebuild);
        connect(m_model, &RoundtableModel::readingRoomChanged, this, [this]() {
            if (!m_subscribing && m_model->readingRoomId().isEmpty()) {
                // A refused automatic read waits for an explicit retry, a
                // tuning change or reconnect, rather than resubmitting here.
                m_failedContext = roomId();
            }
            // The Core refused the read, or the room closed.
            if (!m_subscribing && !m_readId.isEmpty() && m_model->readingRoomId() != m_readId) {
                m_readId.clear();
            }
            rebuild();
        });
        connect(m_model, &RoundtableModel::postResult, this, &RoundtablePanel::onPostResult);
        connect(m_model, &RoundtableModel::historyResult, this,
                [this](const QString& clientId, bool ok, const QJsonArray& states, const QString&) {
            if (clientId != m_historyRequest) { return; }
            m_historyRequest.clear();
            bool unchanged = true;
            if (ok) {
                for (const QJsonValue& value : states) {
                    const QJsonObject state = value.toObject();
                    const qint64 id = state.value(QStringLiteral("id")).toInteger();
                    if (!m_requestedHistoryVersions.contains(id)
                        || m_requestedHistoryVersions.value(id) != m_messageVersions.value(id)) {
                        unchanged = false;
                        continue;
                    }
                    m_historyStates.insert(id, state);
                }
            }
            m_historyFresh = ok && unchanged && historyConnected(m_model);
            rebuildMessages(m_model->room(roomId()));
            showFreshBodies({m_messages});
        });
    }

    m_tick.setInterval(1000);
    connect(&m_tick, &QTimer::timeout, this, &RoundtablePanel::refreshNow);
    m_tick.start();
    rebuild();
}

RoundtablePanel::~RoundtablePanel() = default;

// ---------------------------------------------------------------- following

void RoundtablePanel::followSlice(int sliceId)
{
    m_sliceId = sliceId;
    if (!m_readId.isEmpty()) {
        m_readId.clear();
        if (m_model) {
            m_model->unreadRoom();
        }
    }
    rebuild();
}

void RoundtablePanel::setSliceFrequency(qint64 hz)
{
    setSliceContext(hz, m_sliceFamily);
}

void RoundtablePanel::setSliceContext(qint64 hz, const QString& family)
{
    if (hz == m_sliceHz && family == m_sliceFamily) {
        return;
    }
    m_sliceHz = hz;
    m_sliceFamily = family;
    m_failedContext.clear();
    m_historyRequestedMs = -1;
    m_historyFresh = false;
    m_readId.clear();
    subscribeContext();
    rebuild();
}

QString RoundtablePanel::draftKey() const
{
    return RoundtableModel::isFrequencyContext(roomId())
        ? QStringLiteral("slice:%1:%2").arg(m_sliceId).arg(roomId()) : roomId();
}

void RoundtablePanel::subscribeContext()
{
    if (!m_subscribing && m_model && m_model->frequencyAvailable() && m_readId.isEmpty()
        && m_sliceHz > 0 && !m_sliceFamily.isEmpty()) {
        const QString id = RoundtableModel::frequencyContextId(m_sliceHz, m_sliceFamily);
        if (id == m_failedContext) { return; }
        if (m_model->readingRoomId() != id) {
            m_subscribing = true;
            m_model->readAt(m_sliceHz, m_sliceFamily);
            m_subscribing = false;
        }
    }
}

void RoundtablePanel::readRoom(const QString& roomId)
{
    if (!m_model) {
        return;
    }
    const QString mine = m_model->frequencyAvailable()
        ? RoundtableModel::frequencyContextId(m_sliceHz, m_sliceFamily)
        : m_model->sliceRoomId(m_sliceId);
    if (roomId.isEmpty() || roomId == mine) {
        backToSlice();
        return;
    }
    m_readId = roomId;
    if (RoundtableModel::isFrequencyContext(roomId)) {
        m_subscribing = true;
        const bool accepted = m_model->readAt(roomHz(roomId), roomFamily(roomId));
        m_subscribing = false;
        if (!accepted) { m_readId.clear(); }
    } else {
        m_model->readRoom(roomId);
    }
    rebuild();
}

void RoundtablePanel::backToSlice()
{
    if (m_readId.isEmpty()) {
        if (!m_failedContext.isEmpty()) {
            m_failedContext.clear();
            subscribeContext();
            rebuild();
        }
        return;
    }
    m_readId.clear();
    if (m_model) {
        m_model->unreadRoom();
    }
    m_failedContext.clear();
    subscribeContext();
    rebuild();
}

QString RoundtablePanel::roomId() const
{
    if (!m_readId.isEmpty()) {
        return m_readId;
    }
    if (m_model && (m_model->frequencyAvailable() || !m_retainedMessages.isEmpty())
        && m_sliceHz > 0 && !m_sliceFamily.isEmpty()) {
        return RoundtableModel::frequencyContextId(m_sliceHz, m_sliceFamily);
    }
    return m_model ? m_model->sliceRoomId(m_sliceId) : QString();
}

void RoundtablePanel::setSplit(bool split)
{
    if (split == m_split) {
        return;
    }
    m_split = split;
    placeMessages();
    rebuild();
}

void RoundtablePanel::setPopped(bool popped)
{
    m_popped = popped;
    rebuild();
}

void RoundtablePanel::placeMessages()
{
    auto* mainLayout = static_cast<QVBoxLayout*>(m_scroll->parentWidget()->layout());
    auto* columnLayout = static_cast<QVBoxLayout*>(m_msgColumn->layout());
    if (m_split) {
        m_contentLayout->removeWidget(m_messages);
        m_msgScroll->setWidget(m_messages);
        mainLayout->removeWidget(m_footer);
        columnLayout->addWidget(m_footer);
        m_msgColumn->show();
    } else {
        if (m_msgScroll->widget() == m_messages) {
            m_msgScroll->takeWidget();
        }
        m_messages->setParent(m_content);
        m_contentLayout->insertWidget(m_contentLayout->indexOf(m_earlier) + 1, m_messages);
        m_messages->show();
        columnLayout->removeWidget(m_footer);
        mainLayout->addWidget(m_footer);
        m_msgColumn->hide();
    }
    m_footer->show();
}

QScrollArea* RoundtablePanel::messagesScroll() const
{
    return m_split ? m_msgScroll : m_scroll;
}

bool RoundtablePanel::messagesAtEnd() const
{
    const QScrollBar* bar = messagesScroll()->verticalScrollBar();
    return bar->value() >= bar->maximum() - 4;
}

void RoundtablePanel::scrollMessagesToEnd()
{
    setProperty("roundtableStick", true);
    QScrollArea* area = messagesScroll();
    if (QWidget* inner = area->widget()) {
        if (inner->layout() != nullptr) {
            inner->layout()->activate();
        }
    }
    area->verticalScrollBar()->setValue(area->verticalScrollBar()->maximum());
    checkOnScreen();
}

// ---------------------------------------------------------------- readers

QJsonObject RoundtablePanel::summaryOf(const QString& roomId) const
{
    if (!m_model) {
        return {};
    }
    for (const QJsonValue& value : m_model->rooms()) {
        const QJsonObject summary = value.toObject();
        if (summary.value(QStringLiteral("id")).toString() == roomId) {
            return summary;
        }
    }
    return {};
}

QString RoundtablePanel::myRef(const QJsonObject& room) const
{
    for (const QJsonValue& value : room.value(QStringLiteral("members")).toArray()) {
        const QJsonObject member = value.toObject();
        if (member.value(QStringLiteral("you")).toBool()) {
            return personKey(member);
        }
    }
    return {};
}

QString RoundtablePanel::myCallsign(const QJsonObject& room) const
{
    for (const QJsonValue& value : room.value(QStringLiteral("members")).toArray()) {
        const QJsonObject member = value.toObject();
        if (member.value(QStringLiteral("you")).toBool()) {
            return member.value(QStringLiteral("callsign")).toString();
        }
    }
    QString callsign = m_model ? m_model->stationSetting(QStringLiteral("Callsign")) : QString();
    if (callsign.isEmpty()) {
        callsign = AppSettings::instance().value(QStringLiteral("User/Callsign")).toString();
    }
    return callsign.trimmed().toUpper();
}

bool RoundtablePanel::mineMessage(const QJsonObject& room, const QJsonObject& message) const
{
    if (m_myMessages.contains(message.value(QStringLiteral("id")).toInteger())) {
        return true;
    }
    const QString ref = myRef(room);
    return !ref.isEmpty() && personKey(message) == ref;
}

bool RoundtablePanel::names(const QJsonObject& message, const QString& callsign) const
{
    if (callsign.isEmpty()) {
        return false;
    }
    const QRegularExpression word(QStringLiteral("(?<![A-Za-z0-9])%1(?![A-Za-z0-9])")
                                      .arg(QRegularExpression::escape(callsign)),
                                  QRegularExpression::CaseInsensitiveOption);
    return word.match(message.value(QStringLiteral("text")).toString()).hasMatch();
}

bool RoundtablePanel::tunedHere() const
{
    return m_readId.isEmpty() && !roomId().isEmpty();
}

bool RoundtablePanel::live() const
{
    return m_model && m_model->linkState() == RoundtableModel::LinkState::Live;
}

bool RoundtablePanel::awayHere(const QJsonObject& room) const
{
    for (const QJsonValue& value : room.value(QStringLiteral("members")).toArray()) {
        const QJsonObject member = value.toObject();
        if (member.value(QStringLiteral("you")).toBool()) {
            return member.value(QStringLiteral("away")).toBool();
        }
    }
    return false;
}

QJsonObject RoundtablePanel::openTurn(const QJsonObject& room) const
{
    const QJsonArray now = room.value(QStringLiteral("now")).toArray();
    // The newest open turn is the one on air.
    return now.isEmpty() ? QJsonObject() : now.last().toObject();
}

QStringList RoundtablePanel::nameSuggestions(const QJsonObject& room) const
{
    // Design 8.6: names are picked from those used on that frequency in
    // the last 24 hours (newest first), or typed. Three chips fit beside
    // the box at the docked width.
    QStringList out;
    if (!m_model) {
        return out;
    }
    const QString mine = RoundtableWire::baseCallsign(myCallsign(room));
    for (const QString& name : m_model->recentNames(room.value(QStringLiteral("id")).toString())) {
        if (RoundtableWire::baseCallsign(name) != mine) {
            out.append(name);
        }
        if (out.size() == kNameChips) {
            break;
        }
    }
    return out;
}

QList<qint64> RoundtablePanel::unreadMentionIds(const QJsonObject& room) const
{
    QList<qint64> ids;
    if (!m_model) {
        return ids;
    }
    int count = m_model->mentionCount(room.value(QStringLiteral("id")).toString());
    const QString mine = myCallsign(room);
    const QJsonArray messages = room.value(QStringLiteral("messages")).toArray();
    for (qsizetype i = messages.size() - 1; i >= 0 && count > 0; --i) {
        const QJsonObject message = messages.at(i).toObject();
        const qint64 id = message.value(QStringLiteral("id")).toInteger();
        if (message.value(QStringLiteral("state")).toString(QStringLiteral("shown"))
                != QLatin1String("shown")
            || mineMessage(room, message) || !names(message, mine)) {
            continue;
        }
        --count;
        if (!m_mentionsMarked.contains(id)) {
            ids.prepend(id);
        }
    }
    return ids;
}

// ---------------------------------------------------------------- building

void RoundtablePanel::onRoomChanged(const QString& changed)
{
    if (m_model && m_model->frequencyAvailable() && changed != roomId()) {
        const QJsonArray updates = m_model->room(changed).value(QStringLiteral("messages")).toArray();
        for (const QJsonValue& update : updates) {
            for (qsizetype i = 0; i < m_retainedMessages.size(); ++i) {
                if (m_retainedMessages.at(i).toObject().value(QStringLiteral("id"))
                    == update.toObject().value(QStringLiteral("id"))) {
                    const QJsonObject message = update.toObject();
                    const qint64 id = message.value(QStringLiteral("id")).toInteger();
                    if (!m_seenLiveMessages.contains(id) || m_seenLiveMessages.value(id) != message) {
                        m_messageVersions[id] = ++m_messageVersionSerial;
                        m_historyStates.remove(id);
                        m_seenLiveMessages.insert(id, message);
                    }
                    m_retainedMessages.replace(i, update);
                }
            }
        }
    }
    if (changed == roomId() || !m_retainedMessages.isEmpty()) {
        rebuild();
    }
}

void RoundtablePanel::rebuild()
{
    subscribeContext();
    const QString id = roomId();
    const QString key = draftKey();
    if (id != m_shownId || key != m_shownDraftKey) {
        m_shownDraftKey = key;
        m_shownId = id;
        m_known.clear();
        m_new.clear();
        m_everOnScreen.clear();
        m_onScreenSince.clear();
        m_showMuted.clear();
        m_earlierSignature.clear();
        m_messagesSignature.clear();
        m_nameEditorTurn = 0;
        m_firstShow = true;
        m_notSent->hide();
        m_settingBox = true;
        m_box->setPlainText(m_model && !id.isEmpty() ? m_model->draft(key) : QString());
        m_settingBox = false;
        emit shownRoomChanged(id);
    }

    m_fittedWidth = roomWidth();
    m_readBar->setVisible(isReading());
    m_readLabel->setText(QStringLiteral("Reading %1").arg(shortFrequency(roomHz(id))));
    fitReadBar();

    if (id.isEmpty()) {
        for (QWidget* section : {m_head, m_txLine, m_talker, m_readings, m_earlier, m_messages}) {
            section->hide();
        }
        m_empty->show();
        rebuildEmpty();
        showFreshBodies({m_empty});
        updateConnectionLine({});
        m_connection->show();
        updateFooter();
        return;
    }
    m_empty->hide();

    const QJsonObject room = m_model ? m_model->room(id) : QJsonObject();
    const QJsonObject turn = openTurn(room);
    const bool keyed = roundtableTurnIsYours(room, turn, true);

    m_head->setVisible(!keyed);
    m_talker->setVisible(!keyed);
    m_txLine->setVisible(keyed);
    m_readings->show();
    m_earlier->show();
    m_messages->setVisible(!room.isEmpty());

    rebuildHead(room);
    updateConnectionLine(room);
    // While keyed the TX line stands in for the header; the connection
    // line stays only when Roundtable is not live.
    m_connection->setVisible(!keyed || !live());
    rebuildTalker(room);
    rebuildReadings(room);
    rebuildEarlier(room);
    rebuildMessages(room);
    showFreshBodies({m_head, m_txLine, m_talker, m_readings, m_earlier, m_messages});
    updateFooter();

    const bool wasKeyed = property("roundtableKeyed").toBool();
    setProperty("roundtableKeyed", keyed);
    if (keyed && !wasKeyed) {
        // Readings of you take the room (U3).
        setProperty("roundtableStick", false);
        m_scroll->verticalScrollBar()->setValue(0);
    }

    if (m_firstShow && !room.isEmpty()) {
        m_firstShow = false;
        const QString opensOn = AppSettings::instance()
                                    .value(QStringLiteral("Roundtable/PanelOpensOn"),
                                           QStringLiteral("Readings"))
                                    .toString();
        if (opensOn == QLatin1String("Messages") || m_split) {
            scrollMessagesToEnd();
        } else {
            setProperty("roundtableStick", false);
            m_scroll->verticalScrollBar()->setValue(0);
        }
    }
    checkOnScreen();
}

void RoundtablePanel::rebuildHead(const QJsonObject& room)
{
    const QString id = roomId();
    QWidget* body = freshBody(m_head);
    body->setObjectName(QStringLiteral("roundtableHeaderBody"));
    body->setAttribute(Qt::WA_StyledBackground, true);
    body->setStyleSheet(QStringLiteral("QWidget#roundtableHeaderBody { %1 }").arg(rule()));
    auto* stack = new QVBoxLayout(body);
    stack->setContentsMargins(8, 6, 8, 4);
    stack->setSpacing(4);
    auto* row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);

    const QChar letter(QLatin1Char(static_cast<char>('A' + std::clamp(m_sliceId, 0, 25))));
    const auto notTunedTag = [body]() {
        QLabel* label = makeLabel(body, QStringLiteral("NOT TUNED HERE"), unknownColor(), kSmallPx, true);
        label->setStyleSheet(label->styleSheet()
                             + QStringLiteral(" border: 1px solid %1; padding: 0 4px;"
                                              " border-radius: 2px;")
                                   .arg(role(Style::kAmberBorder, "measured-border")));
        label->setObjectName(QStringLiteral("roundtableTag"));
        return label;
    };
    m_tagInline = nullptr;
    m_tagAbove = nullptr;
    m_modeInline = nullptr;
    m_modeBelow = nullptr;
    QLabel* tag = nullptr;
    if (tunedHere()) {
        tag = makeLabel(body, QString(letter), textColor(), kSmallPx, true);
        tag->setStyleSheet(tag->styleSheet() + sliceBadgeStyle(m_sliceId));
        tag->setObjectName(QStringLiteral("roundtableTag"));
    } else {
        tag = notTunedTag();
        m_tagInline = tag;
        // The same tag on a line of its own, shown instead when the row
        // would not fit the docked width.
        auto* above = new QHBoxLayout();
        above->setContentsMargins(0, 0, 0, 0);
        m_tagAbove = notTunedTag();
        m_tagAbove->hide();
        above->addWidget(m_tagAbove);
        above->addStretch(1);
        stack->addLayout(above);
    }
    stack->addLayout(row);
    row->addWidget(tag);
    QLabel* freq = makeLabel(body, headerFrequency(roomHz(id)), accentColor(), 15, false, true);
    freq->setObjectName(QStringLiteral("roundtableFrequency"));
    row->addWidget(freq);
    const auto modeTag = [body, &id]() {
        QLabel* label = makeLabel(body, modeLabel(roomFamily(id)), role(Style::kTitleText, "title-text"), kSmallPx);
        label->setStyleSheet(label->styleSheet()
                             + QStringLiteral(" border: 1px solid %1; padding: 0 4px; border-radius: 2px;")
                                   .arg(role(Style::kBorder, "border-muted")));
        label->setObjectName(QStringLiteral("roundtableMode"));
        return label;
    };
    QLabel* mode = modeTag();
    m_modeInline = mode;
    row->addWidget(mode);
    row->addStretch(1);

    const QJsonArray members = room.value(QStringLiteral("members")).toArray();
    int count = static_cast<int>(members.size());
    int away = 0;
    for (const QJsonValue& value : members) {
        if (value.toObject().value(QStringLiteral("away")).toBool()) {
            ++away;
        }
    }
    if (room.isEmpty()) {
        count = summaryOf(id).value(QStringLiteral("members")).toInt();
    }
    m_countOneLine = QStringLiteral("%1 in the room").arg(count);
    m_countTwoLines.clear();
    if (away > 0) {
        m_countTwoLines = m_countOneLine + QStringLiteral("\n%1 away").arg(away);
        m_countOneLine += QStringLiteral(" \u00b7 %1 away").arg(away);
    }
    QLabel* countLabel = makeLabel(body, m_countOneLine, dimColor(), kSmallPx);
    countLabel->setObjectName(QStringLiteral("roundtableCount"));
    countLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row->addWidget(countLabel);
    m_countLabel = countLabel;

    QToolButton* pop = makeOwnWindowButton(body);
    pop->setVisible(!m_popped);
    connect(pop, &QToolButton::clicked, this, &RoundtablePanel::popOutRequested);
    row->addWidget(pop);
    m_popButton = pop;

    auto* menu = new QToolButton(body);
    menu->setObjectName(QStringLiteral("roundtableMenuBtn"));
    menu->setText(QStringLiteral("\u22ef"));
    menu->setToolTip(m_popped ? QStringLiteral("Archive, settings and the wide window")
                              : QStringLiteral("Archive, settings and open in its own window"));
    menu->setStyleSheet(iconStyle());
    menu->setFocusPolicy(Qt::ClickFocus);
    connect(menu, &QToolButton::clicked, this, &RoundtablePanel::showPanelMenu);
    row->addWidget(menu);
    // The line under the row: the count, and the mode beside it when even
    // the row without the count does not fit (fitCount).
    auto* below = new QHBoxLayout();
    below->setContentsMargins(0, 0, 0, 0);
    below->setSpacing(row->spacing());
    m_modeBelow = modeTag();
    m_modeBelow->hide();
    below->addWidget(m_modeBelow);
    m_countBelow = makeLabel(body, m_countOneLine, dimColor(), kSmallPx);
    m_countBelow->setObjectName(QStringLiteral("roundtableCount"));
    m_countBelow->hide();
    below->addWidget(m_countBelow);
    below->addStretch(1);
    stack->addLayout(below);
    fitCount();
}

void RoundtablePanel::fitCount()
{
    // The header row fits the 340 px panel: "N in the room · N away" goes
    // onto two lines first, then NOT TUNED HERE moves to a line of its
    // own (not a word-wrapping label: one with a height for its width
    // makes the scroll area lay out mid-rebuild).
    // [Roundtable plan Task 15: and in the narrower real column, last, the
    // count goes under the frequency.] Then, when even the row without the
    // count does not fit (a 10-character frequency at 260 px), the mode
    // joins the count on the line below, as NOT TUNED HERE moves above.
    if (m_countLabel == nullptr) {
        return;
    }
    // Measured, not laid out: nothing here may relayout the content.
    // The row is the stack item that holds the count (the count below the
    // frequency comes after it).
    QLayout* stack = m_countLabel->parentWidget()->layout();
    QHBoxLayout* row = nullptr;
    for (int i = 0; stack != nullptr && i < stack->count(); ++i) {
        auto* candidate = qobject_cast<QHBoxLayout*>(stack->itemAt(i)->layout());
        if (candidate != nullptr && candidate->indexOf(m_countLabel) >= 0) {
            row = candidate;
        }
    }
    if (row == nullptr) {
        return;
    }
    const QMargins margins = m_countLabel->parentWidget()->layout()->contentsMargins();
    // Polished first: before its style sheet applies, the label's font is
    // the application's, not the count's size.
    m_countLabel->ensurePolished();
    const QFontMetrics metrics(m_countLabel->font());
    const int oneLine = metrics.horizontalAdvance(m_countOneLine);
    int twoLines = oneLine;
    if (!m_countTwoLines.isEmpty()) {
        twoLines = 0;
        for (const QString& line : m_countTwoLines.split(QLatin1Char('\n'))) {
            twoLines = std::max(twoLines, metrics.horizontalAdvance(line));
        }
    }
    int others = margins.left() + margins.right();
    int widgets = 0;
    int tagWidth = 0;
    for (int i = 0; i < row->count(); ++i) {
        QWidget* w = row->itemAt(i)->widget();
        if (w == nullptr || (w->isHidden() && w != m_tagInline && w != m_countLabel && w != m_modeInline)) {
            continue;
        }
        ++widgets;
        if (w == m_countLabel) {
            continue;
        }
        w->ensurePolished();
        if (w == m_tagInline) {
            tagWidth = w->sizeHint().width() + row->spacing();
        } else {
            others += w->sizeHint().width();
        }
    }
    others += std::max(0, widgets - 1) * row->spacing() - (tagWidth > 0 ? row->spacing() : 0);
    const int room = roomWidth();
    bool twoLineCount = false;
    bool tagAbove = false;
    if (others + tagWidth + oneLine > room) {
        twoLineCount = !m_countTwoLines.isEmpty();
        tagAbove = m_tagInline != nullptr && others + tagWidth + twoLines > room;
    }
    const QString& text = twoLineCount ? m_countTwoLines : m_countOneLine;
    if (m_countLabel->text() != text) {
        m_countLabel->setText(text);
    }
    if (m_tagInline != nullptr && m_tagAbove != nullptr && m_tagAbove->isHidden() != !tagAbove) {
        m_tagInline->setVisible(!tagAbove);
        m_tagAbove->setVisible(tagAbove);
    }
    // Roundtable plan Task 15: in the narrowest column the count goes
    // under the frequency, on one line (or wrapped), rather than past the
    // edge.
    if (m_countBelow != nullptr) {
        const int countWidth = twoLineCount ? twoLines : oneLine;
        const bool below = others + (tagAbove ? 0 : tagWidth) + countWidth > room;
        if (m_countBelow->text() != m_countOneLine) {
            m_countBelow->setText(m_countOneLine);
        }
        if (m_countBelow->isHidden() == below) {
            m_countLabel->setVisible(!below);
            m_countBelow->setVisible(below);
        }
        // The row without the count: one spacing fewer.
        const bool modeBelow = below && m_modeInline != nullptr && m_modeBelow != nullptr
                            && others - row->spacing() + (tagAbove ? 0 : tagWidth) > room;
        if (m_modeInline != nullptr && m_modeBelow != nullptr && m_modeBelow->isHidden() == modeBelow) {
            m_modeInline->setVisible(!modeBelow);
            m_modeBelow->setVisible(modeBelow);
        }
    }
}

void RoundtablePanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    refitToWidth();
}

int RoundtablePanel::roomWidth() const
{
    // The viewport less a vertical scroll bar whether one shows or not:
    // the fit never changes when the bar comes and goes, and a content
    // minimum between the two widths cannot make the scroll area flip.
    const int viewport = m_scroll->viewport()->width();
    if (viewport <= 0) {
        return width();
    }
    const QScrollBar* bar = m_scroll->verticalScrollBar();
    const int extent = bar->isVisible() ? 0 : style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, bar);
    return viewport - extent;
}

void RoundtablePanel::refitToWidth()
{
    fitCount();
    fitReadBar();
    // The readings columns, the name chips and the name editor are laid
    // out for a width; a new one rebuilds them, once the resize settles.
    if (roomWidth() == m_fittedWidth || m_refitQueued) {
        return;
    }
    m_refitQueued = true;
    QTimer::singleShot(0, this, [this]() {
        m_refitQueued = false;
        if (roomWidth() == m_fittedWidth) {
            return;
        }
        setProperty("roundtableTalkerSig", QString());
        m_earlierSignature.clear();
        rebuild();
    });
}

void RoundtablePanel::fitReadBar()
{
    if (m_readBack == nullptr) {
        return;
    }
    // Measured, as fitCount is: the first line's parts at their size.
    int needed = 16;
    int parts = 0;
    for (int i = 0; i < m_readFirstLine->count(); ++i) {
        QWidget* w = m_readFirstLine->itemAt(i)->widget();
        if (w == nullptr || w == m_readBack || w == m_readBackSeparator) {
            continue;
        }
        w->ensurePolished();
        needed += w->sizeHint().width();
        ++parts;
    }
    m_readBackSeparator->ensurePolished();
    m_readBack->ensurePolished();
    const int spacing = m_readFirstLine->spacing();
    needed += std::max(0, parts - 1) * spacing;
    const int oneLine = needed + 2 * spacing + m_readBackSeparator->sizeHint().width()
                      + m_readBack->sizeHint().width();
    const bool twoLines = oneLine > roomWidth();
    const bool onSecond = m_readSecondLine->indexOf(m_readBack) >= 0;
    if (twoLines == onSecond) {
        return;
    }
    if (twoLines) {
        m_readFirstLine->removeWidget(m_readBack);
        m_readSecondLine->insertWidget(0, m_readBack);
    } else {
        m_readSecondLine->removeWidget(m_readBack);
        m_readFirstLine->insertWidget(m_readFirstLine->indexOf(m_readBackSeparator) + 1, m_readBack);
    }
    m_readBackSeparator->setVisible(!twoLines);
}

void RoundtablePanel::updateConnectionLine(const QJsonObject& room)
{
    using LinkState = RoundtableModel::LinkState;
    const qint64 now = RoundtableModel::nowMs();
    const LinkState state = m_model ? m_model->linkState() : LinkState::Off;
    const qint64 since = m_model ? m_model->linkSince() : 0;
    QString text;
    QString color = dimColor();
    switch (state) {
    case LinkState::Live: {
        qint64 newest = 0;
        for (const QJsonValue& value : room.value(QStringLiteral("members")).toArray()) {
            const QJsonValue reading = value.toObject().value(QStringLiteral("reading"));
            if (reading.isObject()) {
                newest = std::max(newest, reading.toObject().value(QStringLiteral("t")).toInteger());
            }
        }
        text = newest > 0 ? QStringLiteral("Live \u00b7 last reading %1 ago").arg(ageText(now - newest))
                          : QStringLiteral("Live");
        color = talkingColor();
        break;
    }
    case LinkState::Reconnecting:
        text = QStringLiteral("Reconnecting \u00b7 nothing new for %1").arg(ageText(now - since));
        color = unknownColor();
        break;
    case LinkState::Offline:
        text = since > 0 ? QStringLiteral("Offline since %1 UTC").arg(utcText(since))
                         : QStringLiteral("Offline");
        color = unknownColor();
        break;
    case LinkState::Connecting:
        text = QStringLiteral("Connecting");
        break;
    case LinkState::Hidden:
        text = QStringLiteral("You are hidden");
        color = youColor();
        break;
    case LinkState::Off:
        text = QStringLiteral("Roundtable is off");
        break;
    }
    if (m_model && !m_model->frequencyAvailable()
        && (historyConnected(m_model) || m_retainedMessages.isEmpty())) {
        text += QStringLiteral(" · Legacy room server");
    }
    m_connection->setText(text);
    m_connection->setStyleSheet(labelStyle(color, kSmallPx, false, false)
                                + QStringLiteral(" %1").arg(rule()));
}

void RoundtablePanel::rebuildTalker(const QJsonObject& room)
{
    const qint64 now = RoundtableModel::nowMs();
    const QJsonObject turn = openTurn(room);
    const QString kind = turn.value(QStringLiteral("kind")).toString();
    const bool keyed = roundtableTurnIsYours(room, turn, true);
    const bool canName = tunedHere() && live() && !awayHere(room);
    const QJsonArray earlier = room.value(QStringLiteral("earlier")).toArray();
    int hearing = 0;
    for (const QJsonValue& value : room.value(QStringLiteral("members")).toArray()) {
        const QJsonObject member = value.toObject();
        const QJsonValue reading = member.value(QStringLiteral("reading"));
        if (!member.value(QStringLiteral("you")).toBool() && reading.isObject()
            && reading.toObject().value(QStringLiteral("present")).toBool(true)) {
            ++hearing;
        }
    }
    // Rebuilt only when what it shows changes, so a name being typed stays.
    const qint64 start = turn.value(QStringLiteral("start")).toInteger();
    const QString signature = compact(turn) + QString::number(canName) + QString::number(hearing)
                            + compact(earlier.isEmpty() ? QJsonValue(QJsonObject()) : earlier.first())
                            + QString::number(room.isEmpty())
                            + QString::number(turn.isEmpty() && !earlier.isEmpty()
                                                  ? (now - earlier.first().toObject()
                                                               .value(QStringLiteral("end"))
                                                               .toInteger())
                                                        / 60000
                                                  : 0)
                            + compact(room.value(QStringLiteral("members")));
    const QString keyedText = clockText(now - start);
    if (signature == property("roundtableTalkerSig").toString()) {
        for (QLabel* label : findChildren<QLabel*>(QStringLiteral("roundtableKeyedTime"))) {
            label->setText(keyedText);
        }
        return;
    }
    setProperty("roundtableTalkerSig", signature);

    // The TX line (U3).
    {
        QWidget* body = freshBody(m_txLine);
        body->setObjectName(QStringLiteral("roundtableTxLineBody"));
        body->setAttribute(Qt::WA_StyledBackground, true);
        body->setStyleSheet(QStringLiteral("QWidget#roundtableTxLineBody { %1 }").arg(rule()));
        auto* row = new QHBoxLayout(body);
        row->setContentsMargins(8, 6, 8, 6);
        row->setSpacing(8);
        QLabel* tx = makeLabel(body, QStringLiteral("TX"), txTextColor(), kSmallPx, true);
        tx->setStyleSheet(tx->styleSheet() + QStringLiteral(" background: %1; padding: 0 4px;"
                                                            " border-radius: 2px;")
                                                 .arg(txColor()));
        row->addWidget(tx);
        QLabel* who = makeWrapped(body, QStringLiteral("You \u00b7 %1 hear you").arg(hearing), youColor(),
                                  kBodyPx, true);
        who->setObjectName(QStringLiteral("roundtableTxText"));
        row->addWidget(who);
        QLabel* time = makeLabel(body, keyedText, textColor(), kBodyPx, false, true);
        time->setObjectName(QStringLiteral("roundtableKeyedTime"));
        row->addWidget(time);
        row->addStretch(1);
        row->addWidget(makeLabel(body, headerFrequency(roomHz(roomId())), accentColor(), kBodyPx, false, true));
    }
    if (keyed) {
        return;
    }

    QWidget* body = freshBody(m_talker);
    body->setObjectName(QStringLiteral("roundtableTalkerBody"));
    body->setAttribute(Qt::WA_StyledBackground, true);
    body->setStyleSheet(QStringLiteral("QWidget#roundtableTalkerBody { %1 }").arg(rule()));
    auto* col = new QVBoxLayout(body);
    col->setContentsMargins(8, 6, 8, 6);
    col->setSpacing(2);
    if (room.isEmpty()) {
        col->addWidget(makeLabel(body, QStringLiteral("Opening %1").arg(headerFrequency(roomHz(roomId()))),
                                 dimColor(), kBodyPx));
        return;
    }

    const auto gridOf = [&room](const QString& ref) {
        for (const QJsonValue& value : room.value(QStringLiteral("members")).toArray()) {
            if (personKey(value.toObject()) == ref) {
                return value.toObject().value(QStringLiteral("grid")).toString();
            }
        }
        return QString();
    };
    const auto bigRow = [col]() {
        auto* row = new QHBoxLayout();
        row->setSpacing(6);
        col->addLayout(row);
        return row;
    };
    const auto txBadge = [body]() {
        QLabel* tx = makeLabel(body, QStringLiteral("TX"), txTextColor(), kSmallPx, true);
        tx->setStyleSheet(tx->styleSheet() + QStringLiteral(" background: %1; padding: 0 4px;"
                                                            " border-radius: 2px;")
                                                 .arg(txColor()));
        return tx;
    };
    const QJsonArray talkers = turn.value(QStringLiteral("talkers")).toArray();

    if (turn.isEmpty()) {
        col->addWidget(capLabel(body, QStringLiteral("QUIET")));
        QLabel* big = makeLabel(body, QStringLiteral("Nobody is talking"), dimColor(), 14, true, true);
        big->setObjectName(QStringLiteral("roundtableTalkerName"));
        col->addWidget(big);
        if (!earlier.isEmpty()) {
            const QJsonObject last = earlier.first().toObject();
            const qint64 ago = now - last.value(QStringLiteral("end")).toInteger();
            const QJsonArray lastTalkers = last.value(QStringLiteral("talkers")).toArray();
            QString text;
            if (last.value(QStringLiteral("kind")).toString() == QLatin1String("doubled")) {
                text = QStringLiteral("Last turn · Doubled · combined RF · %1 ago").arg(ageText(ago));
            } else if (roundtableTurnIsYours(room, last, false)) {
                text = QStringLiteral("Last turn was yours, %1 ago").arg(ageText(ago));
            } else if (!lastTalkers.isEmpty()
                       && last.value(QStringLiteral("kind")).toString() != QLatin1String("unknown")) {
                text = (m_model && m_model->frequencyAvailable() ? QStringLiteral("Last talker · %1 · %2 ago")
                                                                    : QStringLiteral("Last turn by %1, %2 ago"))
                           .arg(withNth(lastTalkers.first().toObject()), ageText(ago));
            } else {
                text = (m_model && m_model->frequencyAvailable() ? QStringLiteral("Last talker · Unknown · %1 ago")
                                                                    : QStringLiteral("Last turn by an unknown talker, %1 ago")).arg(ageText(ago));
            }
            col->addWidget(makeWrapped(body, text, dimColor(), kSmallPx));
        }
        return;
    }

    if (kind == QLatin1String("doubled")) {
        col->addWidget(capLabel(body, QStringLiteral("TWO AT ONCE")));
        // Roundtable plan Task 15: each talker's part (TX, the name, the
        // "+" after it), then DOUBLED; where they do not fit one line of
        // the room, each talker takes a line of its own.
        QList<QList<QWidget*>> parts;
        for (qsizetype i = 0; i < talkers.size(); ++i) {
            QList<QWidget*> part;
            part.append(txBadge());
            QLabel* name = makeElided(body, withNth(talkers.at(i).toObject()), doubledColor(), 17, true, true);
            name->setObjectName(QStringLiteral("roundtableTalkerName"));
            part.append(name);
            if (i + 1 < talkers.size()) {
                part.append(makeLabel(body, QStringLiteral("+"), dimColor(), 17, true, true));
            }
            parts.append(part);
        }
        QLabel* tag = makeLabel(body, QStringLiteral("DOUBLED"), txTextColor(), kSmallPx, true);
        tag->setStyleSheet(tag->styleSheet() + QStringLiteral(" background: %1; border: 1px solid"
                                                              " %2; padding: 0 4px;")
                                                   .arg(role(Style::kRedBg, "danger-bg"), doubledColor()));
        if (parts.isEmpty()) {
            parts.append(QList<QWidget*>{});
        }
        parts.last().append(tag);
        constexpr int kBigRowSpacing = 6;
        int oneLine = 16 - kBigRowSpacing;
        for (const QList<QWidget*>& part : std::as_const(parts)) {
            for (QWidget* w : part) {
                w->ensurePolished();
                oneLine += w->minimumSizeHint().width() + kBigRowSpacing;
            }
        }
        const bool lineEach = oneLine > roomWidth();
        QHBoxLayout* row = bigRow();
        for (qsizetype i = 0; i < parts.size(); ++i) {
            if (lineEach && i > 0) {
                row->addStretch(1);
                row = bigRow();
            }
            for (QWidget* w : parts.at(i)) {
                row->addWidget(w);
            }
        }
        row->addStretch(1);
        auto* sub = new QHBoxLayout();
        sub->setSpacing(4);
        sub->addWidget(makeLabel(body, QStringLiteral("for"), dimColor(), kSmallPx));
        QLabel* time = makeLabel(body, keyedText, dimColor(), kSmallPx, false, true);
        time->setObjectName(QStringLiteral("roundtableKeyedTime"));
        sub->addWidget(time);
        sub->addWidget(makeWrapped(body, QStringLiteral("\u00b7 each reading is both signals together"),
                                   dimColor(), kSmallPx));
        sub->addStretch(1);
        col->addLayout(sub);
        return;
    }

    if (kind == QLatin1String("unknown")) {
        col->addWidget(capLabel(body, QStringLiteral("TALKING NOW")));
        QLabel* big = makeLabel(body, QStringLiteral("? Unknown"), unknownColor(), 17, true, true);
        big->setObjectName(QStringLiteral("roundtableTalkerName"));
        col->addWidget(big);
        auto* sub = new QHBoxLayout();
        sub->setSpacing(4);
        sub->addWidget(makeLabel(body, QStringLiteral("not on NereusSDR \u00b7 on air"), dimColor(), kSmallPx));
        QLabel* time = makeLabel(body, keyedText, dimColor(), kSmallPx, false, true);
        time->setObjectName(QStringLiteral("roundtableKeyedTime"));
        sub->addWidget(time);
        sub->addStretch(1);
        col->addLayout(sub);
        QStringList given;
        for (const QJsonValue& value : turn.value(QStringLiteral("names")).toArray()) {
            const QJsonObject name = value.toObject();
            given.append(QStringLiteral("%1 (%2)").arg(name.value(QStringLiteral("name")).toString())
                             .arg(name.value(QStringLiteral("count")).toInt()));
        }
        if (!given.isEmpty()) {
            QLabel* named = makeWrapped(body, QStringLiteral("Named %1").arg(given.join(QStringLiteral(", "))),
                                        unknownColor(), kSmallPx);
            named->setObjectName(QStringLiteral("roundtableNamesGiven"));
            col->addWidget(named);
        }
        // NAME THIS TURN: the names in view as chips, and a callsign box.
        auto* names = new QHBoxLayout();
        names->setSpacing(4);
        QLabel* nameCaption = capLabel(body, QStringLiteral("NAME THIS TURN"));
        names->addWidget(nameCaption);
        const qint64 turnId = turn.value(QStringLiteral("id")).toInteger();
        for (const QString& suggestion : nameSuggestions(room)) {
            QPushButton* chip = makeButton(body, suggestion, buttonStyle());
            chip->setObjectName(QStringLiteral("roundtableNameChip"));
            chip->setEnabled(canName);
            chip->setToolTip(canName ? QStringLiteral("Name this turn %1").arg(suggestion)
                                     : QStringLiteral("Tune a slice here to name a turn"));
            connect(chip, &QPushButton::clicked, this,
                    [this, turnId, suggestion]() { submitName(turnId, suggestion); });
            names->addWidget(chip);
        }
        // Roundtable plan Task 15: where the caption and the chips do not
        // fit one line of the room, the caption takes a line of its own.
        int namesWidth = 16 + std::max(0, names->count() - 1) * names->spacing();
        for (int i = 0; i < names->count(); ++i) {
            if (QWidget* w = names->itemAt(i)->widget()) {
                w->ensurePolished();
                namesWidth += w->sizeHint().width();
            }
        }
        if (namesWidth > roomWidth()) {
            names->removeWidget(nameCaption);
            col->addWidget(nameCaption);
        }
        names->addStretch(1);
        col->addLayout(names);
        auto* inputRow = new QHBoxLayout();
        inputRow->setSpacing(4);
        auto* input = new QLineEdit(body);
        input->setObjectName(QStringLiteral("roundtableTalkerNameInput"));
        input->setPlaceholderText(QStringLiteral("Callsign"));
        input->setStyleSheet(inputStyle());
        input->setMaxLength(16);
        input->setEnabled(canName);
        input->setFixedWidth(110);
        inputRow->addWidget(input);
        QPushButton* nameButton = makeButton(body, QStringLiteral("Name"), buttonStyle());
        nameButton->setEnabled(canName);
        connect(nameButton, &QPushButton::clicked, this, [this, turnId, input]() {
            submitName(turnId, input->text());
        });
        connect(input, &QLineEdit::returnPressed, this, [this, turnId, input]() {
            submitName(turnId, input->text());
        });
        inputRow->addWidget(nameButton);
        inputRow->addStretch(1);
        col->addLayout(inputRow);
        return;
    }

    // Talking.
    col->addWidget(capLabel(body, QStringLiteral("TALKING NOW")));
    QHBoxLayout* row = bigRow();
    row->addWidget(txBadge());
    const QJsonObject talker = talkers.isEmpty() ? QJsonObject() : talkers.first().toObject();
    QLabel* name = makeElided(body, withNth(talker), talkingColor(), 17, true, true);
    name->setObjectName(QStringLiteral("roundtableTalkerName"));
    row->addWidget(name);
    row->addWidget(makeLabel(body, talker.value(QStringLiteral("grid")).toString(gridOf(personKey(talker))), dimColor(),
                             kSmallPx));
    row->addStretch(1);
    auto* sub = new QHBoxLayout();
    sub->setSpacing(4);
    sub->addWidget(makeLabel(body, QStringLiteral("keyed"), dimColor(), kSmallPx));
    QLabel* time = makeLabel(body, keyedText, dimColor(), kSmallPx, false, true);
    time->setObjectName(QStringLiteral("roundtableKeyedTime"));
    sub->addWidget(time);
    sub->addWidget(makeWrapped(body, QStringLiteral("\u00b7 on NereusSDR, so the talker is known"),
                               dimColor(), kSmallPx));
    sub->addStretch(1);
    col->addLayout(sub);
}

void RoundtablePanel::rebuildReadings(const QJsonObject& room)
{
    const qint64 now = RoundtableModel::nowMs();
    QWidget* body = freshBody(m_readings);
    auto* col = new QVBoxLayout(body);
    col->setContentsMargins(0, 0, 0, 4);
    col->setSpacing(0);
    if (room.isEmpty()) {
        return;
    }
    QJsonObject turn = openTurn(room);
    if (turn.isEmpty()) {
        const QJsonArray earlier = room.value(QStringLiteral("earlier")).toArray();
        if (!earlier.isEmpty()) { turn = earlier.first().toObject(); }
    }
    const bool frequency = m_model && m_model->frequencyAvailable();
    const bool miles = !m_model || m_model->stationSetting(QStringLiteral("DistanceUnits")) != QLatin1String("km");
    const bool bearing = !m_model || m_model->stationSetting(QStringLiteral("ShowBearing")) != QLatin1String("False");
    const QString unit = miles ? QStringLiteral("mi") : QStringLiteral("km");
    const QString turnKind = turn.value(QStringLiteral("kind")).toString();
    const bool identified = (turnKind == QLatin1String("talking") || turnKind == QLatin1String("known"))
        && turn.value(QStringLiteral("talkers")).toArray().size() == 1;
    const auto pathText = [miles, bearing, identified](const QJsonObject& member) {
        if (!identified) { return QString(); }
        const QJsonObject reading = member.value(QStringLiteral("reading")).toObject();
        const QJsonValue km = reading.contains(QStringLiteral("km"))
            ? reading.value(QStringLiteral("km")) : member.value(QStringLiteral("km"));
        const QJsonValue direction = reading.contains(QStringLiteral("bearingDeg"))
            ? reading.value(QStringLiteral("bearingDeg")) : member.value(QStringLiteral("bearingDeg"));
        QString text = km.isDouble() ? QString::number(std::lround(km.toDouble() / (miles ? 1.609344 : 1.0))) : QString();
        if (bearing && direction.isDouble() && direction.toInt(-1) >= 0 && direction.toInt() < 360) {
            text += QStringLiteral("\n%1°T").arg(direction.toInt(), 3, 10, QLatin1Char('0'));
        }
        return text;
    };
    const QString kind = turn.value(QStringLiteral("kind")).toString();
    const bool keyed = roundtableTurnIsYours(room, turn, true);
    QSet<QString> talkerRefs;
    QStringList talkerNames;
    for (const QJsonValue& value : turn.value(QStringLiteral("talkers")).toArray()) {
        talkerRefs.insert(personKey(value.toObject()));
        talkerNames.append(withNth(value.toObject()));
    }

    QString title;
    if (keyed) {
        title = QStringLiteral("READINGS OF YOU");
    } else if (turn.isEmpty()) {
        title = QStringLiteral("IN THE ROOM");
    } else if (kind == QLatin1String("doubled")) {
        title = QStringLiteral("READINGS OF BOTH");
    } else if (kind == QLatin1String("unknown") || talkerNames.isEmpty()) {
        title = QStringLiteral("READINGS OF THE TALKER");
    } else {
        title = QStringLiteral("READINGS OF %1").arg(talkerNames.first());
    }

    struct Row {
        QJsonObject member;
        bool hasKm = false;
        double km = 0.0;
    };
    QList<Row> rows;
    QStringList others;
    bool anyAway = false;
    for (const QJsonValue& value : room.value(QStringLiteral("members")).toArray()) {
        const QJsonObject member = value.toObject();
        const bool isYou = member.value(QStringLiteral("you")).toBool();
        if (member.value(QStringLiteral("away")).toBool()) {
            anyAway = true;
        }
        if (!isYou) {
            others.append(withNth(member));
        }
        if (talkerRefs.contains(personKey(member)) || (keyed && isYou)
            || !member.value(QStringLiteral("reading")).isObject()) {
            continue;
        }
        const QJsonObject reading = member.value(QStringLiteral("reading")).toObject();
        if (frequency && (turn.isEmpty() || reading.value(QStringLiteral("turn")).toInteger(-1)
                                              != turn.value(QStringLiteral("id")).toInteger(-2))) {
            continue;
        }
        Row row;
        row.member = member;
        const QJsonValue km = member.value(QStringLiteral("km"));
        row.hasKm = km.isDouble() && identified;
        row.km = km.toDouble();
        rows.append(row);
    }
    // Nearest first; rows with no distance follow by callsign (6.8).
    // Hidden, or before the first-run answer, the room has no row of you:
    // your own reading shows here only, marked NOT SENT (R-RT-13).
    const bool unsent = m_model && m_model->linkState() == RoundtableModel::LinkState::Hidden;
    if (unsent && tunedHere() && !keyed && m_ownReadingMs > 0) {
        bool listed = false;
        for (const Row& row : std::as_const(rows)) {
            listed = listed || row.member.value(QStringLiteral("you")).toBool();
        }
        if (!listed) {
            Row own;
            own.member = QJsonObject{
                {QStringLiteral("ref"), QString()},
                {QStringLiteral("callsign"), myCallsign(room)},
                {QStringLiteral("nth"), 1},
                {QStringLiteral("km"), QJsonValue::Null},
                {QStringLiteral("grid"), QString()},
                {QStringLiteral("offsetHz"), 0},
                {QStringLiteral("away"), false},
                {QStringLiteral("you"), true},
                {QStringLiteral("reading"),
                 QJsonObject{{QStringLiteral("dbm"), m_ownDbm},
                             {QStringLiteral("t"), m_ownReadingMs},
                             {QStringLiteral("present"), std::isfinite(m_ownDbm)
                                                             && m_ownDbm >= RoundtableWire::kMinDbm
                                                             && m_ownDbm <= RoundtableWire::kMaxDbm}}}};
            rows.append(own);
        }
    }
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        if (a.hasKm != b.hasKm) {
            return a.hasKm;
        }
        if (a.hasKm && a.km != b.km) {
            return a.km < b.km;
        }
        const QString ac = a.member.value(QStringLiteral("callsign")).toString();
        const QString bc = b.member.value(QStringLiteral("callsign")).toString();
        return ac == bc ? personKey(a.member) < personKey(b.member) : ac < bc;
    });
    const bool byDistance = !rows.isEmpty() && rows.first().hasKm;

    auto* head = new QHBoxLayout();
    head->setContentsMargins(8, 6, 8, 3);
    QLabel* titleLabel = capLabel(body, frequency && !turn.isEmpty() ? QStringLiteral("HEARD BY") : title);
    titleLabel->setObjectName(QStringLiteral("roundtableReadingsTitle"));
    titleLabel->setWordWrap(true);
    titleLabel->setMinimumWidth(0);
    head->addWidget(titleLabel, 1);
    if (!turn.isEmpty() && !rows.isEmpty()) {
        head->addWidget(makeLabel(body, byDistance ? QStringLiteral("nearest first")
                                                   : QStringLiteral("by callsign"),
                                  dim2Color(), kSmallPx));
    }
    col->addLayout(head);

    if (others.isEmpty()) {
        QLabel* lonely = makeLabel(body, QStringLiteral("Nobody else is here. Your readings of the next "
                                                        "talker will appear here."),
                                   dimColor(), kSmallPx);
        lonely->setObjectName(QStringLiteral("roundtableNobodyElse"));
        lonely->setWordWrap(true);
        lonely->setContentsMargins(8, 2, 8, 6);
        col->addWidget(lonely);
        return;
    }
    if (turn.isEmpty()) {
        // Quiet: who is in the room.
        QLabel* list = makeLabel(body, others.join(QStringLiteral("  ")), textColor(), kSmallPx, false, true);
        list->setObjectName(QStringLiteral("roundtableInTheRoom"));
        list->setWordWrap(true);
        list->setContentsMargins(8, 2, 8, 6);
        col->addWidget(list);
        return;
    }

    // The columns at the room's width (the real applet column is 260 px
    // and up, the design's 340 px panel the widest docked shape): the
    // values in each column stay whole, the reading bar takes the slack,
    // and under the design widths the Station column gives way first,
    // its callsigns eliding past 6 characters, then the others down to
    // what they hold, then the gaps.
    const QFont callFont = [body]() {
        QLabel probe(body);
        probe.setStyleSheet(labelStyle(textColor(), kSmallPx, true, true));
        probe.ensurePolished();
        return probe.font();
    }();
    const QFont valueFont = [body]() {
        QLabel probe(body);
        probe.setStyleSheet(labelStyle(textColor(), kSmallPx, false, true));
        probe.ensurePolished();
        return probe.font();
    }();
    const QFontMetrics callMetrics(callFont);
    const QFontMetrics valueMetrics(valueFont);
    // A tag (AWAY, NOT SENT) as the row draws it, border and padding too.
    const auto tagWidth = [body](const QString& text) {
        QLabel probe(text, body);
        probe.setStyleSheet(readingTagStyle());
        probe.ensurePolished();
        return probe.sizeHint().width();
    };
    int callNeed = 0;
    // The call column without the NOT SENT tag, and the tag: where the
    // call and the tag cannot share the column, the tag takes your row's
    // km and Offset cells (empty for you) and the bar the rest.
    int callNeedPlain = 0;
    const int unsentTagWidth = tagWidth(QStringLiteral("NOT SENT"));
    const int awayTagWidth = tagWidth(QStringLiteral("AWAY"));
    int kmNeed = 0;
    if (bearing && identified) { kmNeed = valueMetrics.horizontalAdvance(QStringLiteral("359°T")) + 1; }
    int offsetNeed = 0;
    int valueNeed = valueMetrics.horizontalAdvance(QStringLiteral("none"));
    int ageNeed = 0;
    for (const Row& entry : std::as_const(rows)) {
        const QJsonObject& member = entry.member;
        const QJsonObject reading = member.value(QStringLiteral("reading")).toObject();
        const QString call = withNth(member);
        const QString kept = call.size() > kCallsignKeep ? call.left(kCallsignKeep) + QChar(0x2026) : call;
        // A tag (AWAY, NOT SENT) beside the call: its text, 2 px padding and
        // the border each side, and the gap.
        int tags = 0;
        if (member.value(QStringLiteral("away")).toBool()) {
            tags += awayTagWidth + 3;
        }
        callNeedPlain = std::max(callNeedPlain, callMetrics.horizontalAdvance(kept) + 1 + tags);
        if (member.value(QStringLiteral("you")).toBool() && unsent) {
            tags += unsentTagWidth + 3;
        }
        callNeed = std::max(callNeed, callMetrics.horizontalAdvance(kept) + 1 + tags);
        kmNeed = std::max(kmNeed, valueMetrics.horizontalAdvance(
                                      entry.hasKm ? pathText(member).section(QLatin1Char('\n'), 0, 0)
                                                  : QString()) + 1);
        offsetNeed = std::max(offsetNeed, valueMetrics.horizontalAdvance(
                                              offsetText(member.value(QStringLiteral("offsetHz")).toInteger())) + 1);
        const double dbm = reading.value(QStringLiteral("dbm")).toDouble();
        valueNeed = std::max(valueNeed, valueMetrics.horizontalAdvance(frequency ? QString::number(std::lround(dbm)) : sText(dbm)) + 1);
        ageNeed = std::max(ageNeed, valueMetrics.horizontalAdvance(
                                        ageText(now - reading.value(QStringLiteral("t")).toInteger())) + 1);
    }
    // The column heads stay whole too.
    const QFontMetrics headMetrics([body]() {
        QLabel probe(body);
        probe.setStyleSheet(labelStyle(dim2Color(), kSmallPx, false, false));
        probe.ensurePolished();
        return probe.font();
    }());
    kmNeed = std::max(kmNeed, headMetrics.horizontalAdvance(unit) + 1);
    // The Offset head elides (whole in its tooltip) to its first three
    // letters, never further.
    offsetNeed = std::max(offsetNeed, headMetrics.horizontalAdvance(QStringLiteral("Off") + QChar(0x2026)) + 1);
    ageNeed = std::max(ageNeed, headMetrics.horizontalAdvance(QStringLiteral("Age")) + 1);
    callNeed = std::max(callNeed, headMetrics.horizontalAdvance(QStringLiteral("Stn")) + 1);
    callNeedPlain = std::max(callNeedPlain, headMetrics.horizontalAdvance(QStringLiteral("Stn")) + 1);
    // Up to 2 seconds of ageing between rebuilds ("9 s" to "10 s").
    ageNeed = std::max(ageNeed, valueMetrics.horizontalAdvance(QStringLiteral("59 s")) + 1);
    const int available = roomWidth();
    // At the tightest gaps (4 px margins, 2 px between), does NOT SENT
    // still fit beside the call?
    const bool unsentBeforeBar =
        callNeed != callNeedPlain
        && 2 * 4 + 5 * 2 + kReadingBarMinWidth + callNeed + kmNeed + offsetNeed + valueNeed + ageNeed > available;
    if (unsentBeforeBar) {
        callNeed = callNeedPlain;
    }
    int barMinWidth = kReadingBarMinWidth;
    struct Column {
        int want;
        int need;
    };
    Column cols[] = {{std::max(kCallWidth, callNeed), callNeed},
                     {std::max(kKmWidth, kmNeed), kmNeed},
                     {std::max(kOffsetWidth, offsetNeed), offsetNeed},
                     {std::max(kValueWidth, valueNeed), valueNeed},
                     {std::max(kAgeWidth, ageNeed), ageNeed}};
    int widths[5];
    for (int i = 0; i < 5; ++i) {
        widths[i] = cols[i].want;
    }
    int margin = 8;
    int spacing = 4;
    const auto total = [&widths, &margin, &spacing, &barMinWidth]() {
        int sum = 2 * margin + 5 * spacing + barMinWidth;
        for (const int w : widths) {
            sum += w;
        }
        return sum;
    };
    for (int i = 0; i < 5 && total() > available; ++i) {
        widths[i] = std::max(cols[i].need, widths[i] - (total() - available));
    }
    if (total() > available) {
        spacing = 2;
    }
    if (total() > available) {
        margin = 4;
    }
    if (total() > available) {
        barMinWidth = std::max(kReadingBarNarrowWidth, barMinWidth - (total() - available));
    }
    const int callWidth = widths[0];
    const int kmWidth = widths[1];
    const int offsetWidth = widths[2];
    const int valueWidth = widths[3];
    const int ageWidth = widths[4];

    const auto columns = [margin, spacing](QHBoxLayout* row) {
        row->setContentsMargins(margin, 0, margin, 0);
        row->setSpacing(spacing);
    };
    {
        auto* header = new QHBoxLayout();
        columns(header);
        const auto cell = [body](const QString& text, int width, Qt::Alignment align) {
            QLabel* label = makeElided(body, text, dim2Color(), kSmallPx, false, false, 3);
            if (width > 0) {
                label->setFixedWidth(width);
            }
            label->setAlignment(align | Qt::AlignVCenter);
            return label;
        };
        header->addWidget(cell(frequency ? QStringLiteral("Heard by") : QStringLiteral("Station"), callWidth, Qt::AlignLeft));
        header->addWidget(cell(unit, kmWidth, Qt::AlignRight));
        header->addWidget(cell(QStringLiteral("Offset"), offsetWidth, Qt::AlignRight));
        header->addWidget(cell(frequency ? QStringLiteral("dBm") : QStringLiteral("Reading"), 0, Qt::AlignLeft), 1);
        header->addWidget(cell(QStringLiteral("Age"), ageWidth, Qt::AlignRight));
        col->addLayout(header);
    }

    for (const Row& entry : std::as_const(rows)) {
        const QJsonObject& member = entry.member;
        const QJsonObject reading = member.value(QStringLiteral("reading")).toObject();
        const qint64 age = now - reading.value(QStringLiteral("t")).toInteger();
        const bool stale = age > kStaleReadingMs;
        const bool away = member.value(QStringLiteral("away")).toBool();
        const bool isYou = member.value(QStringLiteral("you")).toBool();
        const bool present = reading.value(QStringLiteral("present")).toBool(true);
        const double dbm = reading.value(QStringLiteral("dbm")).toDouble();

        auto* rowWidget = new QFrame(body);
        rowWidget->setObjectName(QStringLiteral("roundtableReadingRow"));
        rowWidget->setProperty("callsign", member.value(QStringLiteral("callsign")).toString());
        rowWidget->setProperty("stale", stale);
        rowWidget->setProperty("away", away);
        rowWidget->setFixedHeight(bearing && identified ? 32 : 20);
        if (isYou) {
            rowWidget->setStyleSheet(QStringLiteral("QFrame#roundtableReadingRow {"
                                                    " background: %1; }")
                                         .arg(yourRowColor()));
        }
        if (away) {
            rowWidget->setToolTip(QStringLiteral("Nobody at the radio. Its Core keeps reporting what it "
                                                 "hears."));
        }
        rowWidget->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(rowWidget, &QWidget::customContextMenuRequested, this,
                [this, rowWidget, member]() { showReadingMenu(rowWidget, member); });
        auto* row = new QHBoxLayout(rowWidget);
        columns(row);
        const QString text = stale ? staleColor() : (isYou ? youColor() : (away ? awayColor() : textColor()));

        auto* callCell = new QWidget(rowWidget);
        callCell->setObjectName(QStringLiteral("roundtableReadingCallCell"));
        // The row's own background (yours tinted) shows through the cell.
        callCell->setStyleSheet(QStringLiteral("QWidget#roundtableReadingCallCell { background: transparent; }"));
        callCell->setFixedWidth(callWidth);
        auto* callRow = new QHBoxLayout(callCell);
        callRow->setContentsMargins(0, 0, 0, 0);
        callRow->setSpacing(3);
        QLabel* call = makeElided(callCell, withNth(member), text, kSmallPx, true, true);
        call->setObjectName(QStringLiteral("roundtableReadingCall"));
        callRow->addWidget(call);
        if (away) {
            QLabel* awayTag = new QLabel(QStringLiteral("AWAY"), callCell);
            awayTag->setObjectName(QStringLiteral("roundtableAwayTag"));
            awayTag->setStyleSheet(readingTagStyle());
            callRow->addWidget(awayTag);
        }
        QLabel* unsentTag = nullptr;
        if (isYou && unsent) {
            unsentTag = new QLabel(QStringLiteral("NOT SENT"),
                                   unsentBeforeBar ? static_cast<QWidget*>(rowWidget) : callCell);
            unsentTag->setObjectName(QStringLiteral("roundtableNotSentTag"));
            unsentTag->setToolTip(QStringLiteral("Hidden: only you see this reading"));
            unsentTag->setStyleSheet(readingTagStyle());
            if (!unsentBeforeBar) {
                callRow->addWidget(unsentTag);
            }
        }
        callRow->addStretch(1);
        row->addWidget(callCell);

        auto* bar = new ReadingBar(present ? sFraction(dbm) : 0.0, kind == QLatin1String("doubled"), stale,
                                   rowWidget);
        bar->setMinimumWidth(barMinWidth);
        if (unsentTag != nullptr && unsentBeforeBar) {
            // Roundtable plan Task 15: NOT SENT in your row's (empty) km
            // and Offset cells, the bar taking what is left.
            row->addWidget(unsentTag);
            bar->setMinimumWidth(0);
            row->addWidget(bar, 1);
        } else {
            QLabel* km = makeLabel(rowWidget,
                                   pathText(member),
                                   stale ? staleColor() : dimColor(), kSmallPx, false, true);
            km->setToolTip(identified && !talkerNames.isEmpty()
                ? QStringLiteral("%1 → %2 · %3 · initial true bearing")
                    .arg(talkerNames.first(), withNth(member), unit) : QString());
            km->setFixedWidth(kmWidth);
            km->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            row->addWidget(km);
            const qint64 offsetHz = member.value(QStringLiteral("offsetHz")).toInteger();
            QLabel* offset = makeLabel(rowWidget, offsetText(offsetHz),
                                       stale ? staleColor() : (std::llabs(offsetHz) >= 50 ? unknownColor() : dimColor()),
                                       kSmallPx, false, true);
            offset->setFixedWidth(offsetWidth);
            offset->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            row->addWidget(offset);
            row->addWidget(bar, 1);
        }
        QLabel* value = makeLabel(rowWidget, present ? (frequency ? QString::number(std::lround(dbm)) : sText(dbm)) : QStringLiteral("none"), text,
                                  kSmallPx, false, true);
        value->setObjectName(QStringLiteral("roundtableReadingValue"));
        value->setFixedWidth(valueWidth);
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(value);
        // The age turns amber once stale (U4).
        QLabel* ageLabel = makeLabel(rowWidget, ageText(age), stale ? unknownColor() : dimColor(), kSmallPx,
                                     stale, true);
        ageLabel->setObjectName(QStringLiteral("roundtableReadingAge"));
        ageLabel->setFixedWidth(ageWidth);
        ageLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(ageLabel);
        col->addWidget(rowWidget);
    }
    if (anyAway) {
        QLabel* note = makeLabel(body, QStringLiteral("AWAY: nobody at the radio. Its readings still "
                                                      "count, but it can't post or name turns."),
                                 dimColor(), kSmallPx);
        note->setObjectName(QStringLiteral("roundtableAwayNote"));
        note->setWordWrap(true);
        note->setContentsMargins(8, 3, 8, 5);
        col->addWidget(note);
    }
}

void RoundtablePanel::rebuildEarlier(const QJsonObject& room)
{
    const QJsonArray earlier = room.value(QStringLiteral("earlier")).toArray();
    const QString shown = AppSettings::instance()
                              .value(QStringLiteral("Roundtable/EarlierTurnsShown"), QStringLiteral("All"))
                              .toString();
    const bool canName = tunedHere() && live() && !awayHere(room);
    const QString signature = compact(earlier) + shown + QString::number(canName)
                            + QString::number(m_nameEditorTurn) + compact(room.value(QStringLiteral("members")));
    if (signature == m_earlierSignature) {
        return;
    }
    m_earlierSignature = signature;
    const QString focusKey = QApplication::focusWidget() != nullptr
                                 ? QApplication::focusWidget()->property("navKey").toString()
                                 : QString();

    QWidget* body = freshBody(m_earlier);
    auto* col = new QVBoxLayout(body);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(0);
    m_navItems.erase(std::remove_if(m_navItems.begin(), m_navItems.end(),
                                    [](const QPointer<QWidget>& item) {
                                        return item.isNull()
                                            || item->property("navKey").toString().startsWith(
                                                QLatin1String("t:"));
                                    }),
                     m_navItems.end());
    if (earlier.isEmpty()) {
        return;
    }
    const int limit = shown == QLatin1String("All") ? static_cast<int>(earlier.size()) : shown.toInt();

    auto* head = new QHBoxLayout();
    head->setContentsMargins(8, 6, 8, 3);
    head->addWidget(capLabel(body, QStringLiteral("EARLIER TURNS")), 1);
    head->addWidget(makeLabel(body, QStringLiteral("last 30 minutes"), dim2Color(), kSmallPx));
    col->addLayout(head);

    QList<QPointer<QWidget>> turnItems;
    QWidget* focusTarget = nullptr;
    for (qsizetype i = 0; i < earlier.size() && i < limit; ++i) {
        const QJsonObject turn = earlier.at(i).toObject();
        const qint64 id = turn.value(QStringLiteral("id")).toInteger();
        const QString kind = turn.value(QStringLiteral("kind")).toString();
        const bool mine = roundtableTurnIsYours(room, turn, false);
        auto* row = new QFrame(body);
        row->setObjectName(QStringLiteral("roundtableTurnRow"));
        row->setStyleSheet(turnRowStyle());
        row->setFocusPolicy(Qt::ClickFocus);
        row->setProperty("navKey", QStringLiteral("t:%1").arg(id));
        row->setProperty("turnId", id);
        row->setProperty("kind", kind);
        const bool nameable = kind == QLatin1String("unknown");
        row->setProperty("nameable", nameable);
        row->installEventFilter(this);
        row->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(row, &QWidget::customContextMenuRequested, this,
                [this, row, turn]() { showTurnMenu(row, turn); });
        auto* line = new QHBoxLayout(row);
        line->setContentsMargins(8, 3, 8, 3);
        line->setSpacing(6);
        line->addWidget(makeLabel(row, utcText(turn.value(QStringLiteral("start")).toInteger()), dimColor(),
                                  kSmallPx, false, true));
        QStringList talkers;
        for (const QJsonValue& value : turn.value(QStringLiteral("talkers")).toArray()) {
            talkers.append(withNth(value.toObject()));
        }
        QString who;
        QString color = talkingColor();
        if (mine) {
            who = QStringLiteral("You");
            color = youColor();
        } else if (kind == QLatin1String("doubled")) {
            who = talkers.join(QStringLiteral(" + "));
            color = doubledColor();
        } else if (kind == QLatin1String("unknown")) {
            who = QStringLiteral("Unknown");
            color = unknownColor();
        } else {
            who = talkers.isEmpty() ? QStringLiteral("Unknown") : talkers.first();
        }
        QLabel* whoLabel = makeElided(row, who, color, kSmallPx, true, true);
        whoLabel->setObjectName(QStringLiteral("roundtableTurnWho"));
        line->addWidget(whoLabel);
        QStringList given;
        for (const QJsonValue& value : turn.value(QStringLiteral("names")).toArray()) {
            const QJsonObject name = value.toObject();
            given.append(QStringLiteral("%1 (%2)").arg(name.value(QStringLiteral("name")).toString())
                             .arg(name.value(QStringLiteral("count")).toInt()));
        }
        if (!given.isEmpty()) {
            QLabel* named = makeWrapped(row, QStringLiteral("named %1").arg(given.join(QStringLiteral(", "))),
                                        dimColor(), kSmallPx);
            named->setObjectName(QStringLiteral("roundtableTurnNames"));
            line->addWidget(named);
        }
        line->addStretch(1);
        if (nameable) {
            // U9: name a turn after it ends.
            QPushButton* name = makeButton(row, QStringLiteral("Name"), buttonStyle());
            name->setObjectName(QStringLiteral("roundtableTurnNameBtn"));
            name->setEnabled(canName);
            if (!canName) {
                name->setToolTip(QStringLiteral("Tune a slice here to name a turn"));
            }
            connect(name, &QPushButton::clicked, this, [this, row, id]() { openNameEditor(row, id); });
            line->addWidget(name);
        }
        col->addWidget(row);
        turnItems.append(row);
        if (row->property("navKey").toString() == focusKey) {
            focusTarget = row;
        }

        if (id == m_nameEditorTurn && canName) {
            auto* editor = new QWidget(body);
            editor->setObjectName(QStringLiteral("roundtableNameEditor"));
            auto* editLines = new QVBoxLayout(editor);
            editLines->setContentsMargins(8, 2, 8, 4);
            editLines->setSpacing(4);
            auto* edit = new QHBoxLayout();
            edit->setSpacing(4);
            editLines->addLayout(edit);
            for (const QString& suggestion : nameSuggestions(room)) {
                QPushButton* chip = makeButton(editor, suggestion, buttonStyle());
                chip->setObjectName(QStringLiteral("roundtableNameChip"));
                connect(chip, &QPushButton::clicked, this,
                        [this, id, suggestion]() { submitName(id, suggestion); });
                edit->addWidget(chip);
            }
            auto* input = new QLineEdit(editor);
            input->setObjectName(QStringLiteral("roundtableNameInput"));
            input->setPlaceholderText(QStringLiteral("Callsign"));
            input->setStyleSheet(inputStyle());
            input->setMaxLength(16);
            input->setFixedWidth(100);
            input->setProperty("nameEditorFor", id);
            input->installEventFilter(this);
            connect(input, &QLineEdit::returnPressed, this,
                    [this, id, input]() { submitName(id, input->text()); });
            edit->addWidget(input);
            // Roundtable plan Task 15: the box under the chips where one
            // line of the room does not hold them all.
            int editWidth = 16 + std::max(0, edit->count() - 1) * edit->spacing();
            for (int i = 0; i < edit->count(); ++i) {
                if (QWidget* w = edit->itemAt(i)->widget()) {
                    w->ensurePolished();
                    editWidth += std::max(w->sizeHint().width(), w->minimumWidth());
                }
            }
            if (editWidth > roomWidth() && edit->count() > 1) {
                edit->removeWidget(input);
                auto* second = new QHBoxLayout();
                second->addWidget(input);
                second->addStretch(1);
                editLines->addLayout(second);
            }
            edit->addStretch(1);
            col->addWidget(editor);
            focusTarget = input;
        }
    }
    // The turns come before the messages in the arrow order.
    QList<QPointer<QWidget>> merged = turnItems;
    merged.append(m_navItems);
    m_navItems = merged;
    if (focusTarget != nullptr) {
        focusTarget->setFocus(Qt::OtherFocusReason);
    }
}

void RoundtablePanel::rebuildMessages(const QJsonObject& room)
{
    const QJsonArray current = room.value(QStringLiteral("messages")).toArray();
    QJsonArray messages = current;
    if (m_model && (m_model->frequencyAvailable() || !m_retainedMessages.isEmpty())) {
        QHash<qint64, QJsonObject> received;
        for (const QJsonValue& value : m_retainedMessages) {
            received.insert(value.toObject().value(QStringLiteral("id")).toInteger(), value.toObject());
        }
        for (const QJsonValue& value : current) {
            const QJsonObject message = value.toObject();
            const qint64 id = message.value(QStringLiteral("id")).toInteger();
            if (!m_seenLiveMessages.contains(id) || m_seenLiveMessages.value(id) != message) {
                m_messageVersions[id] = ++m_messageVersionSerial;
                m_historyStates.remove(id);
                m_seenLiveMessages.insert(id, message);
            }
            received.insert(id, message);
        }
        QList<qint64> ids = received.keys();
        std::sort(ids.begin(), ids.end());
        while (ids.size() > 200) { ids.removeFirst(); }
        messages = {};
        for (qint64 id : ids) {
            QJsonObject message = received.value(id);
            if (m_historyStates.contains(id)) {
                const QJsonObject state = m_historyStates.value(id);
                message.insert(QStringLiteral("state"), state.value(QStringLiteral("state")));
                message.insert(QStringLiteral("text"), state.value(QStringLiteral("text")));
            }
            messages.append(message);
        }
        const QSet<qint64> kept(ids.cbegin(), ids.cend());
        for (auto it = m_seenLiveMessages.begin(); it != m_seenLiveMessages.end();) {
            it = kept.contains(it.key()) ? std::next(it) : m_seenLiveMessages.erase(it);
        }
        for (auto it = m_historyStates.begin(); it != m_historyStates.end();) {
            it = kept.contains(it.key()) ? std::next(it) : m_historyStates.erase(it);
        }
        for (auto it = m_messageVersions.begin(); it != m_messageVersions.end();) {
            it = kept.contains(it.key()) ? std::next(it) : m_messageVersions.erase(it);
        }
        m_retainedMessages = messages;
    }
    QStringList mineIds;
    for (const qint64 id : std::as_const(m_myMessages)) {
        mineIds.append(QString::number(id));
    }
    std::sort(mineIds.begin(), mineIds.end());
    QStringList shownMuted;
    for (const qint64 id : std::as_const(m_showMuted)) {
        shownMuted.append(QString::number(id));
    }
    std::sort(shownMuted.begin(), shownMuted.end());
    const QString signature = compact(messages) + (m_model ? m_model->muteList().join(QLatin1Char(','))
                                                           : QString())
                            + mineIds.join(QLatin1Char(',')) + shownMuted.join(QLatin1Char(','))
                            + myRef(room) + myCallsign(room) + QString::number(m_historyFresh && historyConnected(m_model));
    if (signature == m_messagesSignature) {
        return;
    }
    m_messagesSignature = signature;
    const bool atEnd = messagesAtEnd();
    const QString focusKey = QApplication::focusWidget() != nullptr
                                 ? QApplication::focusWidget()->property("navKey").toString()
                                 : QString();

    // Arrivals since the room first showed are new (U11).
    for (const QJsonValue& value : messages) {
        const qint64 id = value.toObject().value(QStringLiteral("id")).toInteger();
        if (!m_known.contains(id)) {
            m_known.insert(id);
            if (!m_firstShow) {
                m_new.insert(id);
            }
        }
    }
    const QList<qint64> unread = unreadMentionIds(room);
    const QString mine = myCallsign(room);

    QWidget* body = freshBody(m_messages);
    auto* col = new QVBoxLayout(body);
    col->setContentsMargins(0, 0, 0, 4);
    col->setSpacing(0);
    m_messageRows.clear();
    m_navItems.erase(std::remove_if(m_navItems.begin(), m_navItems.end(),
                                    [](const QPointer<QWidget>& item) {
                                        return item.isNull()
                                            || item->property("navKey").toString().startsWith(
                                                QLatin1String("m:"));
                                    }),
                     m_navItems.end());

    auto* head = new QHBoxLayout();
    head->setContentsMargins(8, 6, 8, 3);
    head->addWidget(capLabel(body, QStringLiteral("MESSAGES")), 1);
    if (!m_retainedMessages.isEmpty() && m_model
        && (!m_historyFresh || !historyConnected(m_model) || !m_model->frequencyAvailable())) {
        const QString text = !historyConnected(m_model) ? QStringLiteral("History offline")
            : !m_model->frequencyAvailable() ? QStringLiteral("History unavailable") : QStringLiteral("History updating");
        auto* status = makeLabel(body, text, dimColor(), kSmallPx);
        status->setObjectName(QStringLiteral("roundtableHistoryStatus"));
        col->addWidget(status);
    }
    int newCount = 0;
    for (const qint64 id : std::as_const(m_new)) {
        if (!m_everOnScreen.contains(id)) {
            ++newCount;
        }
    }
    QLabel* tally = makeLabel(body, newCount > 0 ? QStringLiteral("%1, %2 new").arg(messages.size()).arg(newCount)
                                                 : QString::number(messages.size()),
                              dim2Color(), kSmallPx, false, true);
    tally->setObjectName(QStringLiteral("roundtableMessagesTally"));
    head->addWidget(tally);
    col->addLayout(head);

    if (messages.isEmpty()) {
        QLabel* none = makeLabel(body, QStringLiteral("No messages on %1 so far.")
                                           .arg(headerFrequency(roomHz(roomId()))),
                                 dimColor(), kSmallPx);
        none->setContentsMargins(8, 4, 8, 6);
        col->addWidget(none);
    }

    QWidget* focusTarget = nullptr;
    for (const QJsonValue& value : messages) {
        const QJsonObject message = value.toObject();
        const qint64 id = message.value(QStringLiteral("id")).toInteger();
        const QString callsign = message.value(QStringLiteral("callsign")).toString();
        const QString state = message.value(QStringLiteral("state")).toString(QStringLiteral("shown"));
        const bool isMine = mineMessage(room, message);
        const bool mention = state == QLatin1String("shown") && !isMine && names(message, mine);
        const bool isNew = m_new.contains(id);

        auto* row = new QFrame(body);
        row->setObjectName(QStringLiteral("roundtableMessageRow"));
        row->setFocusPolicy(Qt::ClickFocus);
        row->setProperty("navKey", QStringLiteral("m:%1").arg(id));
        row->setProperty("messageId", id);
        const bool retained = RoundtableModel::isFrequencyContext(roomId())
            && (!message.value(QStringLiteral("hz")).isDouble()
                || message.value(QStringLiteral("family")).toString() != roomFamily(roomId())
                || std::llabs(message.value(QStringLiteral("hz")).toInteger() - roomHz(roomId())) > 5000);
        row->setProperty("retained", retained);
        row->setProperty("isNew", isNew);
        row->setProperty("mention", mention);
        row->setProperty("unreadMention", unread.contains(id));
        row->installEventFilter(this);
        row->setStyleSheet(messageRowStyle(isNew, mention));
        auto* rowCol = new QVBoxLayout(row);
        rowCol->setContentsMargins(8, 4, 8, 4);
        rowCol->setSpacing(2);
        if (retained) {
            rowCol->addWidget(makeLabel(row, QStringLiteral("EARLIER · retained after tuning"),
                                       dim2Color(), kSmallPx));
        }
        m_messageRows.insert(id, row);
        m_navItems.append(row);
        if (row->property("navKey").toString() == focusKey) {
            focusTarget = row;
        }

        // Mute folds that station's messages away for you only.
        if (!isMine && m_model && m_model->muted(callsign) && !m_showMuted.contains(id)) {
            row->setProperty("muted", true);
            auto* fold = new QHBoxLayout();
            fold->setSpacing(4);
            QLabel* text = makeLabel(row, QStringLiteral("Message from %1 hidden, you muted them.")
                                              .arg(withNth(message)),
                                     dim2Color(), kSmallPx);
            text->setObjectName(QStringLiteral("roundtableMutedFold"));
            // Wraps, so the fold never widens the 340 px panel.
            text->setWordWrap(true);
            text->setMinimumWidth(0);
            QFont italic = text->font();
            italic.setItalic(true);
            text->setFont(italic);
            fold->addWidget(text, 1);
            QPushButton* show = makeButton(row, QStringLiteral("Show"), linkStyle());
            show->setObjectName(QStringLiteral("roundtableShowMutedBtn"));
            connect(show, &QPushButton::clicked, this, [this, id]() {
                m_showMuted.insert(id);
                rebuild();
            });
            fold->addWidget(show);
            QPushButton* unmute = makeButton(row, QStringLiteral("Unmute %1").arg(callsign), linkStyle());
            connect(unmute, &QPushButton::clicked, this, [this, callsign]() {
                if (m_model) {
                    m_model->unmute(callsign);
                }
            });
            fold->addWidget(unmute);
            rowCol->addLayout(fold);
            col->addWidget(row);
            continue;
        }

        auto* top = new QHBoxLayout();
        top->setSpacing(6);
        QLabel* who = makeElided(row, withNth(message), isMine ? youColor() : textColor(), kSmallPx, true, true);
        who->setObjectName(QStringLiteral("roundtableMessageWho"));
        top->addWidget(who);
        if (message.value(QStringLiteral("hz")).isDouble()) {
            QLabel* frequency = makeLabel(row, shortFrequency(message.value(QStringLiteral("hz")).toInteger()),
                                          dimColor(), kSmallPx, false, true);
            frequency->setObjectName(QStringLiteral("roundtableMessageFrequency"));
            frequency->setToolTip(QStringLiteral("Sender frequency when this message was sent"));
            top->addWidget(frequency);
        }
        const QJsonValue dbm = message.value(QStringLiteral("dbm"));
        if (dbm.isDouble()) {
            QLabel* chip = makeLabel(row, sText(dbm.toDouble()), dimColor(), kSmallPx, false, true);
            chip->setToolTip(QStringLiteral("Their reading of the talker when they sent it"));
            chip->setStyleSheet(chip->styleSheet()
                                + QStringLiteral(" background: %1; border: 1px solid %2;"
                                                 " border-radius: 3px; padding: 0 4px;")
                                      .arg(role(Style::kInsetBg, "inset"), role(Style::kInsetBorder, "inset-border")));
            top->addWidget(chip);
        }
        top->addStretch(1);
        QLabel* fresh = makeLabel(row, QStringLiteral("new"), accentColor(), kSmallPx, true);
        fresh->setObjectName(QStringLiteral("roundtableNewMark"));
        fresh->setVisible(isNew);
        top->addWidget(fresh);
        top->addWidget(makeLabel(row, utcText(message.value(QStringLiteral("t")).toInteger()), dim2Color(),
                                 kSmallPx, false, true));
        if (state == QLatin1String("shown")) {
            auto* more = new QToolButton(row);
            more->setObjectName(QStringLiteral("roundtableMessageMenuBtn"));
            more->setText(QStringLiteral("\u22ef"));
            more->setToolTip(isMine ? QStringLiteral("Delete") : QStringLiteral("Mute, report or copy"));
            more->setStyleSheet(iconStyle());
            more->setFocusPolicy(Qt::NoFocus);
            connect(more, &QToolButton::clicked, this,
                    [this, more, message]() { showMessageMenu(more, message); });
            top->addWidget(more);
        }
        rowCol->addLayout(top);

        QLabel* text = nullptr;
        if (state == QLatin1String("removed")) {
            text = makeLabel(row, QStringLiteral("Message removed"), dim2Color(), kSmallPx);
        } else if (state == QLatin1String("deleted")) {
            text = makeLabel(row, QStringLiteral("Deleted by sender"), dim2Color(), kSmallPx);
        } else if (state == QLatin1String("expired")) {
            text = makeLabel(row, QStringLiteral("Message removed after a year"), dim2Color(), kSmallPx);
        } else {
            text = makeLabel(row, QString(), textColor(), kBodyPx);
            text->setTextFormat(Qt::RichText);
            text->setText(formatted(message.value(QStringLiteral("text")).toString()));
            text->setOpenExternalLinks(true);
            text->setTextInteractionFlags(Qt::TextBrowserInteraction);
        }
        if (state != QLatin1String("shown")) {
            QFont italic = text->font();
            italic.setItalic(true);
            text->setFont(italic);
        }
        text->setObjectName(QStringLiteral("roundtableMessageText"));
        text->setWordWrap(true);
        text->setFocusPolicy(Qt::NoFocus);
        rowCol->addWidget(text);
        col->addWidget(row);
    }
    // In the messages column the rows sit at the top, not spread out.
    col->addStretch(1);

    if (focusTarget != nullptr) {
        focusTarget->setFocus(Qt::OtherFocusReason);
    }
    if (atEnd && !m_firstShow) {
        scrollMessagesToEnd();
    }
}

void RoundtablePanel::rebuildEmpty()
{
    QWidget* body = freshBody(m_empty);
    auto* col = new QVBoxLayout(body);
    col->setContentsMargins(0, 0, 0, 6);
    col->setSpacing(2);
    auto* head = new QHBoxLayout();
    head->setContentsMargins(8, 6, 8, 4);
    head->setSpacing(6);
    const QChar letter(QLatin1Char(static_cast<char>('A' + std::clamp(m_sliceId, 0, 25))));
    QLabel* tag = makeLabel(body, QString(letter), textColor(), kSmallPx, true);
    tag->setStyleSheet(tag->styleSheet() + sliceBadgeStyle(m_sliceId));
    head->addWidget(tag);
    if (m_sliceHz > 0) {
        head->addWidget(makeLabel(body, headerFrequency(m_sliceHz), accentColor(), 15, false, true));
    }
    head->addStretch(1);
    QToolButton* pop = makeOwnWindowButton(body);
    pop->setVisible(!m_popped);
    connect(pop, &QToolButton::clicked, this, &RoundtablePanel::popOutRequested);
    head->addWidget(pop);
    auto* menu = new QToolButton(body);
    menu->setObjectName(QStringLiteral("roundtableMenuBtn"));
    menu->setText(QStringLiteral("\u22ef"));
    menu->setToolTip(m_popped ? QStringLiteral("Archive, settings and the wide window")
                              : QStringLiteral("Archive, settings and open in its own window"));
    menu->setStyleSheet(iconStyle());
    menu->setFocusPolicy(Qt::ClickFocus);
    connect(menu, &QToolButton::clicked, this, &RoundtablePanel::showPanelMenu);
    head->addWidget(menu);
    col->addLayout(head);

    QLabel* title = makeLabel(body, QStringLiteral("Nobody on Roundtable is tuned here."), textColor(),
                              kBodyPx, true);
    title->setObjectName(QStringLiteral("roundtableEmptyTitle"));
    title->setWordWrap(true);
    title->setContentsMargins(8, 4, 8, 0);
    col->addWidget(title);
    QLabel* how = makeLabel(body, QStringLiteral("Open a callsign on the panadapter to Read its frequency or explicitly tune there."),
                            dimColor(), kSmallPx);
    how->setWordWrap(true);
    how->setContentsMargins(8, 0, 8, 4);
    col->addWidget(how);

    // The rooms on the slice's band, else the busiest.
    QList<QJsonObject> rooms;
    const QString band = m_sliceHz > 0 ? RoundtableWire::bandOf(m_sliceHz) : QString();
    if (m_model) {
        for (const QJsonValue& value : m_model->rooms()) {
            const QJsonObject summary = value.toObject();
            const qint64 hz = summary.value(QStringLiteral("hz")).toInteger();
            if (band.isEmpty() || RoundtableWire::bandOf(hz) == band) {
                rooms.append(summary);
            }
        }
    }
    if (band.isEmpty()) {
        std::stable_sort(rooms.begin(), rooms.end(), [](const QJsonObject& a, const QJsonObject& b) {
            return a.value(QStringLiteral("members")).toInt() > b.value(QStringLiteral("members")).toInt();
        });
    }
    for (qsizetype i = 0; i < rooms.size() && i < kEmptyRooms; ++i) {
        const QJsonObject summary = rooms.at(i);
        const QString id = summary.value(QStringLiteral("id")).toString();
        QPushButton* read = makeButton(body,
                                       QStringLiteral("%1  %2  \u00b7  %3")
                                           .arg(headerFrequency(roomHz(id)), modeLabel(roomFamily(id)))
                                           .arg(summary.value(QStringLiteral("members")).toInt()),
                                       buttonStyle());
        read->setObjectName(QStringLiteral("roundtableEmptyRoom"));
        read->setProperty("roomId", id);
        read->setToolTip(QStringLiteral("Read this room without tuning"));
        connect(read, &QPushButton::clicked, this, [this, id]() { readRoom(id); });
        auto* line = new QHBoxLayout();
        line->setContentsMargins(8, 0, 8, 0);
        line->addWidget(read);
        line->addStretch(1);
        col->addLayout(line);
    }
}

void RoundtablePanel::updateFooter()
{
    const QString id = roomId();
    const QJsonObject room = m_model && !id.isEmpty() ? m_model->room(id) : QJsonObject();
    m_footer->setVisible(!id.isEmpty());
    const bool tuned = tunedHere();
    const bool isLive = live();
    const bool away = awayHere(room);
    const QString text = m_box->toPlainText();
    const qsizetype points = text.toUcs4().size();
    const qsizetype lines = text.isEmpty() ? 0 : text.count(QLatin1Char('\n')) + 1;
    const bool fits = points <= kMessageLimit && lines <= kMessageLines;

    m_box->setEnabled(tuned && !away);
    m_box->setPlaceholderText(tuned ? QStringLiteral("Message %1").arg(headerFrequency(roomHz(id)))
                                    : QStringLiteral("Tune a slice to %1 to post here")
                                          .arg(headerFrequency(roomHz(id))));
    // While not live, Send is off and the draft stays; nothing is queued.
    m_send->setEnabled(tuned && isLive && !away && !text.trimmed().isEmpty() && fits);

    QString hint;
    if (!tuned) {
        hint = QStringLiteral("Tune a slice here to post. You can read everything.");
    } else if (away) {
        hint = QStringLiteral("Your station is away, so it can't post or name turns.");
    } else if (m_model && m_model->linkState() == RoundtableModel::LinkState::Hidden) {
        hint = QStringLiteral("You are hidden, so nothing you type is sent.");
    } else if (!isLive) {
        hint = QStringLiteral("Send is off until Roundtable is live. Your draft stays here.");
    } else {
        const QJsonObject turn = openTurn(room);
        const QJsonArray talkers = turn.value(QStringLiteral("talkers")).toArray();
        if (turn.isEmpty()) {
            hint = QStringLiteral("Sent with no reading, nobody is talking");
        } else if (roundtableTurnIsYours(room, turn, true)) {
            hint = QStringLiteral("Sent as transmitting");
        } else {
            // The mockup's compose hint (placement.html composeBlock): who
            // you hear, and your reading of them as an S unit.
            QString who;
            if (turn.value(QStringLiteral("kind")).toString() == QLatin1String("unknown")
                || talkers.isEmpty()) {
                const QJsonArray given = turn.value(QStringLiteral("names")).toArray();
                who = given.isEmpty()
                          ? QStringLiteral("unknown")
                          : given.first().toObject().value(QStringLiteral("name")).toString();
            } else {
                QStringList calls;
                for (const QJsonValue& value : talkers) {
                    calls.append(withNth(value.toObject()));
                }
                who = calls.join(QStringLiteral(" + "));
            }
            hint = QStringLiteral("Sent with your reading: hearing %1").arg(who);
            for (const QJsonValue& value : room.value(QStringLiteral("members")).toArray()) {
                const QJsonObject member = value.toObject();
                const QJsonValue reading = member.value(QStringLiteral("reading"));
                if (member.value(QStringLiteral("you")).toBool() && reading.isObject()) {
                    hint += QStringLiteral(" · %1")
                                .arg(sText(reading.toObject().value(QStringLiteral("dbm")).toDouble()));
                }
            }
        }
    }
    m_hint->setText(hint);

    // The counter from 400 code points (design 11.1).
    if (points >= kCounterFrom || lines > kMessageLines) {
        m_counter->setText(lines > kMessageLines
                               ? QStringLiteral("%1 lines, up to %2").arg(lines).arg(kMessageLines)
                               : QStringLiteral("%1 / %2").arg(points).arg(kMessageLimit));
        m_counter->setStyleSheet(labelStyle(fits ? dimColor() : unknownColor(), kSmallPx, !fits, true));
        m_counter->show();
    } else {
        m_counter->hide();
    }
    updatePills();
}

void RoundtablePanel::updatePills()
{
    const QString id = roomId();
    const QJsonObject room = m_model && !id.isEmpty() ? m_model->room(id) : QJsonObject();
    const QList<qint64> unread = unreadMentionIds(room);
    // A mention pill comes first (U6, U11).
    m_mentionPill->setText(QStringLiteral("@%1 A message names you \u2193").arg(unread.size()));
    m_mentionPill->setVisible(!unread.isEmpty());
    int waiting = 0;
    for (const qint64 messageId : std::as_const(m_new)) {
        if (!m_everOnScreen.contains(messageId)) {
            ++waiting;
        }
    }
    m_newPill->setText(QStringLiteral("%1 new \u2193").arg(waiting));
    m_newPill->setVisible(waiting > 0 && !messagesAtEnd());
}

void RoundtablePanel::refreshNow()
{
    refreshHistory();
    const QString id = roomId();
    if (id.isEmpty()) {
        updateConnectionLine({});
        return;
    }
    const QJsonObject room = m_model ? m_model->room(id) : QJsonObject();
    updateConnectionLine(room);
    rebuildTalker(room);
    rebuildReadings(room);
    showFreshBodies({m_txLine, m_talker, m_readings});
    checkOnScreen();
}

void RoundtablePanel::refreshHistory()
{
    if (!m_model || !m_model->frequencyAvailable() || !historyConnected(m_model) || m_retainedMessages.isEmpty()) { return; }
    const qint64 now = RoundtableModel::nowMs();
    if (!m_historyRequest.isEmpty()) {
        if (now - m_historyRequestedMs < 30000) { return; }
        m_historyRequest.clear();
    }
    if (m_historyRequestedMs >= 0 && now - m_historyRequestedMs < 10000) { return; }
    QJsonArray ids;
    m_requestedHistoryVersions.clear();
    for (const QJsonValue& value : m_retainedMessages) {
        const qint64 id = value.toObject().value(QStringLiteral("id")).toInteger();
        if (id > 0 && !m_requestedHistoryVersions.contains(id)) {
            ids.append(id);
            m_requestedHistoryVersions.insert(id, m_messageVersions.value(id));
        }
    }
    if (ids.isEmpty()) { return; }
    m_historyRequest = QStringLiteral("history-%1-%2").arg(reinterpret_cast<quintptr>(this), 0, 16)
                                                       .arg(++m_historySerial);
    m_historyRequestedMs = now;
    const QString request = m_historyRequest;
    if (!m_model->requestHistory(ids, request)) { m_historyRequest.clear(); }
}

void RoundtablePanel::setOwnReading(double dbm, qint64 takenMs)
{
    m_ownDbm = dbm;
    m_ownReadingMs = takenMs;
    if (m_model && m_model->linkState() == RoundtableModel::LinkState::Hidden && tunedHere()) {
        rebuildReadings(m_model->room(roomId()));
        showFreshBodies({m_readings});
    }
}

void RoundtablePanel::checkOnScreen()
{
    const QString id = roomId();
    if (id.isEmpty() || !m_model) {
        updatePills();
        return;
    }
    const qint64 now = RoundtableModel::nowMs();
    const QJsonObject room = m_model->room(id);
    const QList<qint64> unread = unreadMentionIds(room);
    QScrollArea* area = messagesScroll();
    const QRect view = area->viewport()->rect();
    QSet<qint64> watched = m_new;
    for (const qint64 messageId : unread) {
        watched.insert(messageId);
    }
    for (const qint64 messageId : std::as_const(watched)) {
        QWidget* row = m_messageRows.value(messageId);
        if (row == nullptr) {
            continue;
        }
        const QRect place(row->mapTo(area->viewport(), QPoint(0, 0)), row->size());
        const bool exposed = QApplication::activeModalWidget() == nullptr
            || QApplication::activeModalWidget()->isAncestorOf(this);
        const bool onScreen = exposed && isVisible() && row->isVisible() && view.contains(place);
        if (!onScreen) {
            m_onScreenSince.remove(messageId);
            continue;
        }
        m_everOnScreen.insert(messageId);
        const qint64 since = m_onScreenSince.value(messageId, -1);
        if (since < 0) {
            m_onScreenSince.insert(messageId, now);
            continue;
        }
        if (now - since < kOnScreenMs) {
            continue;
        }
        if (m_new.remove(messageId)) {
            row->setProperty("isNew", false);
            row->setStyleSheet(messageRowStyle(false, row->property("mention").toBool()));
            if (QLabel* mark = row->findChild<QLabel*>(QStringLiteral("roundtableNewMark"))) {
                mark->hide();
            }
        }
        if (unread.contains(messageId) && !m_mentionsMarked.contains(messageId)) {
            m_mentionsMarked.insert(messageId);
            row->setProperty("unreadMention", false);
            // Read on one device is read on all (U6); the message keeps its
            // cyan mark.
            for (const QJsonValue& value : room.value(QStringLiteral("messages")).toArray()) {
                const QJsonObject message = value.toObject();
                if (message.value(QStringLiteral("id")).toInteger() == messageId) {
                    m_model->markMentionRead(message.value(QStringLiteral("t")).toInteger());
                }
            }
        }
    }
    if (QLabel* tally = m_messages->findChild<QLabel*>(QStringLiteral("roundtableMessagesTally"))) {
        int waiting = 0;
        for (const qint64 messageId : std::as_const(m_new)) {
            if (!m_everOnScreen.contains(messageId)) {
                ++waiting;
            }
        }
        const int total = static_cast<int>(room.value(QStringLiteral("messages")).toArray().size());
        tally->setText(waiting > 0 ? QStringLiteral("%1, %2 new").arg(total).arg(waiting)
                                   : QString::number(total));
    }
    updatePills();
}

// ---------------------------------------------------------------- actions

void RoundtablePanel::send()
{
    const QString id = roomId();
    const QString text = m_box->toPlainText().trimmed();
    if (!m_model || id.isEmpty() || text.isEmpty() || !m_send->isEnabled()) {
        return;
    }
    const QString clientId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_pending.insert(clientId, Pending{id, draftKey(), text});
    m_notSent->hide();
    m_settingBox = true;
    m_box->clear();
    m_settingBox = false;
    m_model->setDraft(draftKey(), QString());
    updateFooter();
    scrollMessagesToEnd();
    if (m_model->frequencyAvailable()) {
        if (!m_model->postAt(m_sliceId, m_sliceHz, m_sliceFamily, text, clientId)) {
            onPostResult(clientId, false, QStringLiteral("Frequency context was refused. Your draft stays here."));
        }
    } else {
        m_model->post(id, text, clientId);
    }
}

void RoundtablePanel::onPostResult(const QString& clientId, bool ok, const QString& idOrReason)
{
    const auto it = m_pending.find(clientId);
    if (it == m_pending.end()) {
        return;
    }
    const Pending pending = it.value();
    m_pending.erase(it);
    if (ok) {
        m_myMessages.insert(idOrReason.toLongLong());
        m_messagesSignature.clear();
        rebuild();
        return;
    }
    // A failed send keeps the text (design 11.1).
    if (m_model && m_model->draft(pending.draftKey).isEmpty()) {
        m_model->setDraft(pending.draftKey, pending.text);
    }
    if (pending.draftKey == m_shownDraftKey) {
        if (m_box->toPlainText().isEmpty()) {
            m_settingBox = true;
            m_box->setPlainText(pending.text);
            m_settingBox = false;
        }
        m_notSentReason = idOrReason;
        m_notSent->setToolTip(idOrReason);
        m_notSent->show();
        updateFooter();
    }
}

void RoundtablePanel::openNameEditor(QWidget* /*row*/, qint64 turnId)
{
    m_nameEditorTurn = turnId;
    rebuild();
    if (auto* input = findChild<QLineEdit*>(QStringLiteral("roundtableNameInput"))) {
        input->setFocus(Qt::OtherFocusReason);
    }
}

void RoundtablePanel::submitName(qint64 turnId, const QString& name)
{
    const QString callsign = name.trimmed().toUpper();
    if (!m_model || callsign.isEmpty() || turnId <= 0 || !tunedHere() || !live()
        || awayHere(m_model->room(roomId()))) {
        return;
    }
    m_model->nameTurn(QString::number(turnId), callsign);
    if (m_nameEditorTurn == turnId) {
        m_nameEditorTurn = 0;
        m_earlierSignature.clear();
        rebuild();
        for (QWidget* item : std::as_const(m_navItems)) {
            if (item != nullptr && item->property("turnId").toLongLong() == turnId) {
                item->setFocus(Qt::OtherFocusReason);
            }
        }
    }
}

void RoundtablePanel::showMessageMenu(QWidget* anchor, const QJsonObject& message)
{
    if (!m_model) {
        return;
    }
    const QJsonObject room = m_model->room(roomId());
    const QString id = QString::number(message.value(QStringLiteral("id")).toInteger());
    const QString callsign = message.value(QStringLiteral("callsign")).toString();
    const QString text = message.value(QStringLiteral("text")).toString();
    auto* menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("roundtableMessageMenu"));
    menu->setAttribute(Qt::WA_DeleteOnClose);
    QPointer<RoundtableModel> model = m_model;
    if (mineMessage(room, message)) {
        menu->addAction(QStringLiteral("Delete"), this, [model, id]() {
            if (model) {
                model->deleteMessage(id);
            }
        });
    } else {
        menu->addAction(QStringLiteral("Mute %1").arg(callsign), this, [model, callsign]() {
            if (model) {
                model->mute(callsign);
            }
        });
        QMenu* report = menu->addMenu(QStringLiteral("Report"));
        const QList<std::pair<QString, QString>> reasons{
            {QStringLiteral("Spam"), QStringLiteral("spam")},
            {QStringLiteral("Abuse"), QStringLiteral("abuse")},
            {QStringLiteral("Off topic"), QStringLiteral("offTopic")},
            {QStringLiteral("Other"), QStringLiteral("other")}};
        for (const auto& [label, reason] : reasons) {
            report->addAction(label, this, [model, id, reason]() {
                if (model) {
                    model->report(QStringLiteral("message"), id, reason);
                }
            });
        }
        menu->addAction(QStringLiteral("Copy"), this,
                        [text]() { QGuiApplication::clipboard()->setText(text); });
    }
    menu->popup(anchor->mapToGlobal(QPoint(0, anchor->height())));
}

void RoundtablePanel::showReadingMenu(QWidget* row, const QJsonObject& member)
{
    if (!m_model || member.value(QStringLiteral("you")).toBool()) {
        return;
    }
    const QString callsign = member.value(QStringLiteral("callsign")).toString();
    const QString target = member.value(QStringLiteral("sourceRoom")).toString(roomId())
                         + QLatin1Char('/') + member.value(QStringLiteral("ref")).toString();
    auto* menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("roundtableReadingMenu"));
    menu->setAttribute(Qt::WA_DeleteOnClose);
    QPointer<RoundtableModel> model = m_model;
    menu->addAction(m_model->muted(callsign) ? QStringLiteral("Unmute %1").arg(callsign)
                                             : QStringLiteral("Mute %1").arg(callsign),
                    this, [model, callsign]() {
        if (model) {
            if (model->muted(callsign)) {
                model->unmute(callsign);
            } else {
                model->mute(callsign);
            }
        }
    });
    menu->addAction(QStringLiteral("Report false readings"), this, [model, target]() {
        if (model) {
            model->report(QStringLiteral("station"), target, QStringLiteral("falseReadings"));
        }
    });
    menu->popup(row->mapToGlobal(QPoint(0, row->height())));
}

void RoundtablePanel::showTurnMenu(QWidget* row, const QJsonObject& turn)
{
    if (!m_model || turn.value(QStringLiteral("names")).toArray().isEmpty()) {
        return;
    }
    const QString id = QString::number(turn.value(QStringLiteral("id")).toInteger());
    auto* menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("roundtableTurnMenu"));
    menu->setAttribute(Qt::WA_DeleteOnClose);
    QPointer<RoundtableModel> model = m_model;
    menu->addAction(QStringLiteral("Report a wrong name"), this, [model, id]() {
        if (model) {
            model->report(QStringLiteral("turn"), id, QStringLiteral("wrongName"));
        }
    });
    menu->popup(row->mapToGlobal(QPoint(0, row->height())));
}

void RoundtablePanel::showPanelMenu()
{
    auto* menu = new QMenu(this);
    menu->setObjectName(QStringLiteral("roundtablePanelMenu"));
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->addAction(QStringLiteral("Archive"), this, &RoundtablePanel::archiveRequested);
    menu->addAction(QStringLiteral("Settings"), this, &RoundtablePanel::settingsRequested);
    if (m_popped) {
        menu->addAction(QStringLiteral("Wide window"), this, &RoundtablePanel::widePresetRequested);
    } else {
        menu->addAction(QStringLiteral("Open in its own window"), this,
                        &RoundtablePanel::popOutRequested);
    }
    auto* button = findChild<QToolButton*>(QStringLiteral("roundtableMenuBtn"));
    QWidget* anchor = button != nullptr && button->isVisible() ? static_cast<QWidget*>(button) : this;
    menu->popup(anchor->mapToGlobal(QPoint(0, anchor->height())));
}

void RoundtablePanel::jumpToMention()
{
    const QString id = roomId();
    if (!m_model || id.isEmpty()) {
        return;
    }
    const QList<qint64> unread = unreadMentionIds(m_model->room(id));
    if (unread.isEmpty()) {
        return;
    }
    if (QWidget* row = m_messageRows.value(unread.first())) {
        setProperty("roundtableStick", false);
        messagesScroll()->ensureWidgetVisible(row, 0, 40);
        row->setFocus(Qt::OtherFocusReason);
    }
    checkOnScreen();
}

// ---------------------------------------------------------------- keyboard

void RoundtablePanel::focusFirstItem()
{
    for (QWidget* item : std::as_const(m_navItems)) {
        if (item != nullptr && item->isVisible()) {
            item->setFocus(Qt::TabFocusReason);
            messagesScroll()->ensureWidgetVisible(item);
            return;
        }
    }
    if (m_box->isEnabled()) {
        m_box->setFocus(Qt::TabFocusReason);
        return;
    }
    // Nothing to move onto: the panel itself holds the keyboard, so Escape
    // still steps back.
    setFocus(Qt::OtherFocusReason);
}

void RoundtablePanel::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    // Tab moves into the panel (U13): onto its first earlier turn or
    // message.
    if (event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason) {
        focusFirstItem();
    }
}

void RoundtablePanel::moveFocus(int step)
{
    QList<QWidget*> items;
    for (QWidget* item : std::as_const(m_navItems)) {
        if (item != nullptr && item->isVisible()) {
            items.append(item);
        }
    }
    if (items.isEmpty()) {
        return;
    }
    const qsizetype at = items.indexOf(QApplication::focusWidget());
    const qsizetype next = at < 0 ? 0 : std::clamp<qsizetype>(at + step, 0, items.size() - 1);
    QWidget* target = items.at(next);
    target->setFocus(Qt::OtherFocusReason);
    QScrollArea* area = target->property("navKey").toString().startsWith(QLatin1String("m:"))
                            ? messagesScroll()
                            : m_scroll;
    area->ensureWidgetVisible(target, 0, 10);
    if (area == messagesScroll()) {
        setProperty("roundtableStick", messagesAtEnd());
    }
}

void RoundtablePanel::escapePressed()
{
    if (m_popped) {
        emit stepBackRequested();
        return;
    }
    // Docked (U13): back to your slice when reading another room,
    // otherwise out of the panel.
    if (isReading()) {
        backToSlice();
        focusFirstItem();
        return;
    }
    leavePanel();
}

void RoundtablePanel::leavePanel()
{
    for (QWidget* next = nextInFocusChain(); next != nullptr && next != this;
         next = next->nextInFocusChain()) {
        if (!isAncestorOf(next) && next->isVisibleTo(next->window()) && next->isEnabled()
            && (next->focusPolicy() & Qt::TabFocus) == Qt::TabFocus) {
            next->setFocus(Qt::TabFocusReason);
            return;
        }
    }
    if (QWidget* focused = QApplication::focusWidget(); focused != nullptr && isAncestorOf(focused)) {
        focused->clearFocus();
    }
}

bool RoundtablePanel::eventFilter(QObject* watched, QEvent* event)
{
    // The room's width is the scroll's viewport, which settles after the
    // panel's own resize.
    if (event->type() == QEvent::Resize && watched == m_scroll->viewport()) {
        refitToWidth();
        return false;
    }
    if (event->type() != QEvent::KeyPress) {
        return QWidget::eventFilter(watched, event);
    }
    auto* key = static_cast<QKeyEvent*>(event);
    auto* widget = qobject_cast<QWidget*>(watched);
    if (watched == this) {
        if (key->key() == Qt::Key_Escape) {
            escapePressed();
            return true;
        }
        return false;
    }
    if (watched == m_box) {
        if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)
            && !(key->modifiers() & Qt::ShiftModifier)) {
            send();
            return true;
        }
        if (key->key() == Qt::Key_Escape) {
            escapePressed();
            return true;
        }
        return false;
    }
    if (widget != nullptr && widget->property("nameEditorFor").isValid()) {
        if (key->key() == Qt::Key_Escape) {
            const qint64 turnId = widget->property("nameEditorFor").toLongLong();
            m_nameEditorTurn = 0;
            rebuild();
            for (QWidget* item : std::as_const(m_navItems)) {
                if (item != nullptr && item->property("turnId").toLongLong() == turnId) {
                    item->setFocus(Qt::OtherFocusReason);
                }
            }
            return true;
        }
        return false;
    }
    if (widget == nullptr || !widget->property("navKey").isValid()) {
        return QWidget::eventFilter(watched, event);
    }
    switch (key->key()) {
    case Qt::Key_Up:
        moveFocus(-1);
        return true;
    case Qt::Key_Down:
        moveFocus(1);
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        // Enter on an unknown turn opens Name (U13).
        if (widget->property("nameable").toBool() && tunedHere() && live()) {
            openNameEditor(widget, widget->property("turnId").toLongLong());
            return true;
        }
        return false;
    case Qt::Key_Escape:
        escapePressed();
        return true;
    default:
        return false;
    }
}

} // namespace NereusSDR
