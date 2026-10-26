// src/gui/containers/WorkspaceHeader.cpp

// --- From src/gui/applets/GridCellWidget.cpp ---
// =================================================================
// src/gui/applets/GridCellWidget.cpp  (Longpath)
// =================================================================
// Siehe GridCellWidget.h — Kopfleiste plus N Inhalte.
//
// Die Kopfleiste ist Zeile fuer Zeile das, was
// AppletPanelWidget::wrapWithTitleBar bisher gebaut hat (Griff ⋮⋮ in
// kTextScale 10 px, Titel in kTitleText 10 px fett, Style::
// titleBarStyle(), Hoehe kTitleBarH bzw. 22 mit nachgestelltem
// Widget). Schritt 1 soll dasselbe Bild ergeben; wer hier etwas
// aendert, aendert das Aussehen jedes Applets auf einmal.
// =================================================================

// =================================================================
// src/gui/containers/WorkspaceHeader.cpp  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   src/gui/applets/GridCellWidget.cpp at revision
//   551576e30c42731c6d51ce005c5f23ec38be391c,
//   https://github.com/oe5sos/Longpath, primary author Martin Fischer
//   (OE5SOS). The upstream banner is copied above.
//
//   Upstream source has no top-of-file copyright or GPL header; the
//   project-level LICENSE (GPLv3) applies.
//
// See WorkspaceHeader.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-07 - Written for the workspace visual review by J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-08 - The source chip is a click target: it opens the source
//                 menu. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-10-08 - The entry header carries a pop-out button left of its
//                 options button. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-08 - Both headers end with a hide button (Longpath's close
//                 cross, last and rightmost); it hides, never deletes.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-11 - The padlock and setLocked are gone: the workspace look has
//                 no Lock setting. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-11 - The group header carries an eye button left of Pop out:
//                 open while the header is kept shown, crossed out while it
//                 hides when not arranging. Drawn in code so it takes the
//                 header's glyph colours. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-10-11 - A floating container's group header carries a pin button
//                 left of the return arrow: the app's pin art when the
//                 window is not kept on top, the pinned art when it is,
//                 in the header's glyph colours. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - The group header's title shortens with "…" (its
//                 full text in a tooltip) instead of raising the
//                 container's minimum width past the side strip's
//                 thickness. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
// =================================================================

#include "WorkspaceHeader.h"

#include "gui/styles/WorkspacePalette.h"
#include "gui/styles/WorkspaceStyle.h"
#include "gui/widgets/AppIcon.h"

#include <QAbstractButton>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>

namespace NereusSDR {

namespace {

// Workspace review proposal (Task 3 brief), not Longpath values.
constexpr int kGroupGripWidth = 14;
constexpr int kStripeWidth = 3;                 // From Longpath src/gui/applets/GridCellWidget.cpp:73 [@551576e3]
constexpr int kGroupButtonWidth = 16;           // From Longpath src/gui/applets/GridCellWidget.cpp:155 [@551576e3]
constexpr int kGroupButtonHeight = 14;          // From Longpath src/gui/applets/GridCellWidget.cpp:155 [@551576e3]
constexpr int kEntryButtonWidth = 14;
constexpr int kEntryButtonHeight = 12;
constexpr int kEntryGripWidth = 12;
constexpr int kGroupGripPx = 11;                // From Longpath src/gui/applets/GridCellWidget.cpp:83 [@551576e3]
constexpr int kEntryGripPx = 10;
constexpr int kChipHeight = 13;
constexpr int kChipRadius = 4;
constexpr int kChipPadding = 4;
constexpr int kChipBorderAlpha = 153;           // 60 % of 255
constexpr int kHeaderTopRadius = 9;             // kGlassPanelRadius - 1, From Longpath src/gui/applets/GridCellWidget.cpp:63 [@551576e3]
constexpr int kOverlayPlateAlpha = 224;

const QString kGripGlyph = QStringLiteral("⋮⋮");   // From Longpath src/gui/applets/GridCellWidget.cpp:81 [@551576e3]

QString headerButtonCss()
{
    // From Longpath src/gui/applets/GridCellWidget.cpp:144-152 [@551576e3]
    return QStringLiteral(
        "QPushButton { background: transparent; border: none;"
        "  color: %1; font-size: 11px; padding: 0; }"
        "QPushButton:hover { background: %2; color: %3;"
        "  border-radius: %4px; }"
        "QPushButton:disabled { color: %5; }")
        .arg(Workspace::role("text-scale", Workspace::kTextScale),
             Workspace::role("button-hover", Workspace::kButtonHover),
             Workspace::role("text", Workspace::kTextPrimary))
        .arg(Workspace::kGlassChipRadius)   // nie Radius 3 (Hausstil)
        .arg(Workspace::role("text-inactive", Workspace::kTextInactive));
}

QPushButton* makeButton(const QString& glyph, int w, int h, const QString& tip, QWidget* parent)
{
    auto* b = new QPushButton(glyph, parent);
    b->setFixedSize(w, h);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::TabFocus);
    b->setToolTip(tip);
    b->setStyleSheet(headerButtonCss());
    return b;
}

// The eye: a lens outline with a pupil, and crossed out with a stroke when
// the header is not kept shown. Drawn in code, not from an SVG, so it takes
// the same colours as the glyph buttons beside it (headerButtonCss): the
// scale text colour, the primary text colour on the hover plate, and the
// inactive colour when disabled. The geometry is in a 16 x 14 box and is
// scaled to the button.
constexpr double kEyeBoxWidth = 16.0;
constexpr double kEyeBoxHeight = 14.0;
constexpr double kEyeHalfWidth = 5.5;
constexpr double kEyeLidRise = 5.0;      // control point offset of each lid
constexpr double kEyePupilRadius = 1.8;
constexpr double kEyeStroke = 1.25;
constexpr double kEyeSlashStroke = 1.4;
constexpr double kEyeSlashGap = 2.6;     // the cleared band the slash sits in
constexpr double kEyeSlashReach = 4.5;   // from the centre, on each axis

class WorkspaceEyeButton final : public QAbstractButton {
public:
    explicit WorkspaceEyeButton(QWidget* parent) : QAbstractButton(parent)
    {
        setCheckable(true);
        setAttribute(Qt::WA_Hover, true);
    }
    QSize sizeHint() const override { return size(); }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const bool hovered = isEnabled() && underMouse();
        if (hovered) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(Workspace::role("button-hover", Workspace::kButtonHover)));
            p.drawRoundedRect(rect(), Workspace::kGlassChipRadius, Workspace::kGlassChipRadius);
        }
        const QColor colour(!isEnabled() ? Workspace::role("text-inactive", Workspace::kTextInactive)
                            : hovered    ? Workspace::role("text", Workspace::kTextPrimary)
                                         : Workspace::role("text-scale", Workspace::kTextScale));
        p.scale(width() / kEyeBoxWidth, height() / kEyeBoxHeight);
        const QPointF c(kEyeBoxWidth / 2.0, kEyeBoxHeight / 2.0);
        QPainterPath lens;
        lens.moveTo(c.x() - kEyeHalfWidth, c.y());
        lens.quadTo(c.x(), c.y() - kEyeLidRise, c.x() + kEyeHalfWidth, c.y());
        lens.quadTo(c.x(), c.y() + kEyeLidRise, c.x() - kEyeHalfWidth, c.y());
        // Top left to bottom right, so it never echoes the pop-out arrow
        // beside it.
        const QPointF slashFrom(c.x() - kEyeSlashReach, c.y() - kEyeSlashReach);
        const QPointF slashTo(c.x() + kEyeSlashReach, c.y() + kEyeSlashReach);
        if (!isChecked()) {
            // Clear a band along the slash so it reads as a stroke across
            // the eye, not as part of it.
            QPainterPath band;
            band.moveTo(slashFrom);
            band.lineTo(slashTo);
            QPainterPathStroker stroker;
            stroker.setWidth(kEyeSlashGap);
            QPainterPath all;
            all.addRect(QRectF(0, 0, kEyeBoxWidth, kEyeBoxHeight));
            p.setClipPath(all.subtracted(stroker.createStroke(band)));
        }
        p.setPen(QPen(colour, kEyeStroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(lens);
        p.setPen(Qt::NoPen);
        p.setBrush(colour);
        p.drawEllipse(c, kEyePupilRadius, kEyePupilRadius);
        if (!isChecked()) {
            p.setClipping(false);
            p.setPen(QPen(colour, kEyeSlashStroke, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(slashFrom, slashTo);
        }
    }
};

// The pin: the app's own pin and pinned art (AppIcon), drawn as a
// silhouette in the colours of the glyph buttons beside it. AppIcon gives
// the art in its own colours and cannot tint it, so the art's shape is
// taken from its alpha and filled here: the scale text colour, the primary
// text colour on the hover plate, the inactive colour when disabled.
const QString kPinArt = QStringLiteral("pin");
const QString kPinnedArt = QStringLiteral("pinned");

// The pin's outline is taken this many times larger than it is shown.
constexpr int kPinOversample = 4;
// A pixel of the larger outline counts as pin at this alpha or more.
constexpr int kPinCoverCut = 128;

class WorkspacePinButton final : public QAbstractButton {
public:
    explicit WorkspacePinButton(QWidget* parent) : QAbstractButton(parent)
    {
        setCheckable(true);
        setAttribute(Qt::WA_Hover, true);
    }
    QSize sizeHint() const override { return size(); }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const bool hovered = isEnabled() && underMouse();
        if (hovered) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(Workspace::role("button-hover", Workspace::kButtonHover)));
            p.drawRoundedRect(rect(), Workspace::kGlassChipRadius, Workspace::kGlassChipRadius);
        }
        const QColor colour(!isEnabled() ? Workspace::role("text-inactive", Workspace::kTextInactive)
                            : hovered    ? Workspace::role("text", Workspace::kTextPrimary)
                                         : Workspace::role("text-scale", Workspace::kTextScale));
        const int px = WorkspaceGroupHeader::kHeaderPinIconPx;
        const qreal dpr = devicePixelRatioF();
        // The art is a shaded, full-colour pushpin. Its outline is taken
        // larger than shown and cut at half cover, which drops the soft
        // ground shadow and the faint edges, then filled with the header's
        // glyph colour and brought down to size, so the pin has the weight
        // and colour of the glyphs beside it.
        const QPixmap art =
            AppIcon::pixmap(isChecked() ? kPinnedArt : kPinArt, px * kPinOversample, dpr);
        if (art.isNull()) {
            return;
        }
        QImage shape = art.toImage().convertToFormat(QImage::Format_ARGB32);
        const QRgb solid = colour.rgb();
        for (int y = 0; y < shape.height(); ++y) {
            QRgb* line = reinterpret_cast<QRgb*>(shape.scanLine(y));
            for (int x = 0; x < shape.width(); ++x) {
                line[x] = qAlpha(line[x]) >= kPinCoverCut ? solid : qRgba(0, 0, 0, 0);
            }
        }
        const int target = qRound(px * dpr);
        shape = shape.scaled(target, target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        shape.setDevicePixelRatio(dpr);
        p.drawImage(QPointF((width() - px) / 2.0, (height() - px) / 2.0), shape);
    }
};

QLabel* makeGrip(int px, const char* colour, const char* hover, int w, int h, QWidget* parent)
{
    auto* grip = new QLabel(kGripGlyph, parent);
    grip->setObjectName(QStringLiteral("workspaceGrip"));
    grip->setAlignment(Qt::AlignCenter);
    grip->setFixedSize(w, h);
    grip->setCursor(Qt::OpenHandCursor);
    grip->setAttribute(Qt::WA_Hover, true);
    grip->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: %2px; background: transparent; }"
        "QLabel:hover { color: %3; }")
                            .arg(QString::fromLatin1(colour))
                            .arg(px)
                            .arg(QString::fromLatin1(hover)));
    return grip;
}

// The group header's title. It asks for its full text's width but makes do
// with less: given less, it shortens the text with "…" and shows the
// full text as a tooltip. A long title therefore never raises the
// header's, and so the container's, minimum width past what a side strip
// is designed to give it.
class ElidedHeaderTitle final : public QLabel {
public:
    explicit ElidedHeaderTitle(QWidget* parent) : QLabel(parent) {}

    void setFullText(const QString& text)
    {
        m_full = text;
        updateGeometry();
        refresh();
    }
    QString fullText() const { return m_full; }

    QSize sizeHint() const override
    {
        return QSize(widthFor(m_full), QLabel::sizeHint().height());
    }
    QSize minimumSizeHint() const override
    {
        return QSize(widthFor(QString(QChar(0x2026))), QLabel::minimumSizeHint().height());
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QLabel::resizeEvent(event);
        refresh();
    }

private:
    int widthFor(const QString& text) const
    {
        const QMargins m = contentsMargins();
        return fontMetrics().horizontalAdvance(text) + m.left() + m.right() + 2 * margin();
    }
    void refresh()
    {
        const QString shown = fontMetrics().elidedText(m_full, Qt::ElideRight, contentsRect().width() - 2 * margin());
        if (shown != text()) {
            setText(shown);
        }
        setToolTip(shown == m_full ? QString() : m_full);
    }

    QString m_full;
};

} // namespace

// ---- WorkspaceSourceChip ----------------------------------------------------

WorkspaceSourceChip::WorkspaceSourceChip(QWidget* parent) : QWidget(parent)
{
    setFont(Workspace::monoFont(font(), Workspace::kFontCaption));
    setFixedHeight(kChipHeight);
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("Source"));
    hide();
}

void WorkspaceSourceChip::mousePressEvent(QMouseEvent* event)
{
    m_pressed = event->button() == Qt::LeftButton;
    event->accept();
}

void WorkspaceSourceChip::mouseReleaseEvent(QMouseEvent* event)
{
    const bool click = m_pressed && event->button() == Qt::LeftButton
                       && rect().contains(event->position().toPoint());
    m_pressed = false;
    event->accept();
    if (click) {
        emit clicked();
    }
}

void WorkspaceSourceChip::setSource(const QString& text, const QColor& colour)
{
    m_text = text;
    m_colour = colour;
    setFixedWidth(sizeHint().width());
    setVisible(!text.isEmpty());
    update();
}

QSize WorkspaceSourceChip::sizeHint() const
{
    const QFontMetrics fm(font());
    return {fm.horizontalAdvance(m_text) + 2 * kChipPadding + 2, kChipHeight};
}

void WorkspaceSourceChip::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    QColor border = m_colour;
    border.setAlpha(kChipBorderAlpha);
    p.setPen(QPen(border, 1.0));
    p.setBrush(QColor(Workspace::role("tile-active-bg", Workspace::kTileActiveBg)));
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), kChipRadius, kChipRadius);
    p.setPen(m_colour);
    p.drawText(rect(), Qt::AlignCenter, m_text);
}

// ---- WorkspaceCornerGrip ----------------------------------------------------

WorkspaceCornerGrip::WorkspaceCornerGrip(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("workspaceCornerGrip"));
    setFixedSize(kSize, kSize);
    setCursor(Qt::SizeFDiagCursor);
    setToolTip(tr("Drag to resize"));
}

void WorkspaceCornerGrip::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(Workspace::role("text-scale", Workspace::kTextScale)));
    // Three diagonals of dots toward the corner: 1, 2 and 3 dots.
    constexpr double kStep = 3.5;
    constexpr double kDot = 1.0;
    for (int diagonal = 0; diagonal < 3; ++diagonal) {
        for (int i = 0; i <= diagonal; ++i) {
            const double x = kSize - 2.0 - i * kStep;
            const double y = kSize - 2.0 - (diagonal - i) * kStep;
            p.drawEllipse(QPointF(x, y), kDot, kDot);
        }
    }
}

// ---- WorkspaceGroupHeader ---------------------------------------------------

WorkspaceGroupHeader::WorkspaceGroupHeader(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("workspaceGroupHeader"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(kHeight);
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);

    auto* row = new QHBoxLayout(this);
    // From Longpath src/gui/applets/GridCellWidget.cpp:66-67 [@551576e3]
    row->setContentsMargins(2, 0, 4, 0);
    row->setSpacing(4);

    // Die gelbe Anfassmarke, wie an den eigenen Fenstern und wie in
    // der Vorlage. Sie sagt „hier anfassen" — ein Kopf ohne diese Marke sieht
    // aus wie einer, den man nicht anfassen kann.
    auto* stripe = new QLabel(this);
    stripe->setObjectName(QStringLiteral("workspaceStripe"));
    stripe->setFixedSize(kStripeWidth, kHeight);
    // Aus der Palette: derselbe Ton wie an den Fensterleisten und an
    // den Containern (Style::kAmberText). Hier stand #d8a13a — eine
    // eigene Zahl fuer dieselbe Marke.
    stripe->setStyleSheet(QStringLiteral("background: %1;")
                              .arg(Workspace::role("measured", Workspace::kAmberText)));
    row->addWidget(stripe);

    m_grip = makeGrip(kGroupGripPx, Workspace::kTextScale, Workspace::kTextPrimary,
                      kGroupGripWidth, kHeight, this);
    m_grip->setToolTip(tr("Drag to move"));
    row->addWidget(m_grip);

    // NereusSDR: the title shortens with "…" when the header is too
    // narrow for it (ElidedHeaderTitle).
    m_title = new ElidedHeaderTitle(this);
    m_title->setObjectName(QStringLiteral("workspaceGroupTitle"));
    // Versal mit weiter Laufweite (Hausstil Regel 3, umgesetzt mit der
    // Richtung "Glas & Tiefe" am 2026-09-17). Die Groesse kommt aus
    // capsFont, nicht aus dem Stylesheet — dort gewinnt sie sonst
    // gegen jedes setFont darunter (HAUSSTIL.md, die Falle).
    m_title->setFont(Workspace::capsFont(font(), Workspace::kFontCaption));
    m_title->setStyleSheet(QStringLiteral("QLabel { color: %1; background: transparent; }")
                               .arg(Workspace::role("label-mid", Workspace::kLabelMid)));
    row->addWidget(m_title);

    m_chip = new WorkspaceSourceChip(this);
    connect(m_chip, &WorkspaceSourceChip::clicked, this, &WorkspaceGroupHeader::sourceClicked);
    row->addWidget(m_chip);

    row->addStretch(1);

    // The eye, left of Pop out, at the size of the glyph buttons. It shows
    // the saved choice: a click asks for the other one and the owner sets
    // the button from what was saved.
    m_keepShown = new WorkspaceEyeButton(this);
    m_keepShown->setObjectName(QStringLiteral("workspaceKeepShown"));
    m_keepShown->setFixedSize(kGroupButtonWidth, kGroupButtonHeight);
    m_keepShown->setCursor(Qt::PointingHandCursor);
    m_keepShown->setFocusPolicy(Qt::TabFocus);
    connect(m_keepShown, &QAbstractButton::clicked, this, &WorkspaceGroupHeader::keepShownToggled);
    row->addWidget(m_keepShown);
    m_keepShown->setChecked(true);
    setKeepShown(true);

    // The pin, left of the return arrow, only while the container floats.
    // It shows the container's state: a click asks and the owner sets it.
    m_pin = new WorkspacePinButton(this);
    m_pin->setObjectName(QStringLiteral("workspacePin"));
    m_pin->setFixedSize(kGroupButtonWidth, kGroupButtonHeight);
    m_pin->setCursor(Qt::PointingHandCursor);
    m_pin->setFocusPolicy(Qt::TabFocus);
    connect(m_pin, &QAbstractButton::clicked, this, &WorkspaceGroupHeader::pinToggled);
    row->addWidget(m_pin);
    m_pin->hide();
    setPinned(false);

    m_popOut = makeButton(QStringLiteral("↗"), kGroupButtonWidth, kGroupButtonHeight,
                          tr("Pop out"), this);
    m_popOut->setObjectName(QStringLiteral("workspacePopOut"));
    connect(m_popOut, &QPushButton::clicked, this, &WorkspaceGroupHeader::popOutClicked);
    row->addWidget(m_popOut);

    m_options = makeButton(QStringLiteral("⋯"), kGroupButtonWidth, kGroupButtonHeight,
                           tr("Options"), this);
    m_options->setObjectName(QStringLiteral("workspaceOptions"));
    connect(m_options, &QPushButton::clicked, this, &WorkspaceGroupHeader::optionsClicked);
    row->addWidget(m_options);

    // From Longpath src/gui/applets/GridCellWidget.cpp:170-180 [@551576e3]
    // ("Ausblenden": the cross hides, it never deletes; the last button,
    // rightmost). NereusSDR: it comes back from Containers > Hidden.
    m_hide = makeButton(QStringLiteral("\u2715"), kGroupButtonWidth, kGroupButtonHeight,
                        tr("Hide. Bring it back from the Containers menu."), this);
    m_hide->setObjectName(QStringLiteral("workspaceHide"));
    connect(m_hide, &QPushButton::clicked, this, &WorkspaceGroupHeader::hideClicked);
    row->addWidget(m_hide);

    applyStyle();
}

void WorkspaceGroupHeader::applyStyle()
{
    // From Longpath src/gui/applets/GridCellWidget.cpp:59-63 [@551576e3]
    QString css = QStringLiteral(
        "QWidget#workspaceGroupHeader { %1 border-top-left-radius: %2px;"
        " border-top-right-radius: %2px; }")
                      .arg(Workspace::titleBarStyle())
                      .arg(kHeaderTopRadius);
    if (m_arrange) {
        css += QStringLiteral("QWidget#workspaceGroupHeader { border-bottom: 1px solid %1; }")
                   .arg(Workspace::role("accent", Workspace::kAccent));
    }
    setStyleSheet(css);
}

void WorkspaceGroupHeader::setTitle(const QString& title)
{
    static_cast<ElidedHeaderTitle*>(m_title)->setFullText(title);
}
QString WorkspaceGroupHeader::title() const { return static_cast<ElidedHeaderTitle*>(m_title)->fullText(); }
void WorkspaceGroupHeader::setSource(const QString& text, const QColor& colour)
{
    m_chip->setSource(text, colour);
}
QString WorkspaceGroupHeader::sourceText() const { return m_chip->isVisibleTo(this) ? m_chip->text() : QString(); }
void WorkspaceGroupHeader::setFloating(bool floating)
{
    m_popOut->setText(floating ? QStringLiteral("↙") : QStringLiteral("↗"));
    m_popOut->setToolTip(floating ? tr("Return") : tr("Pop out"));
    m_pin->setVisible(floating);
}
void WorkspaceGroupHeader::setPinned(bool pinned)
{
    m_pin->setChecked(pinned);
    // The art by name, for the icon property the app's icons carry.
    AppIcon::apply(m_pin, pinned ? kPinnedArt : kPinArt, kHeaderPinIconPx);
    // The tooltip says what a click will do.
    m_pin->setToolTip(pinned ? tr("Stop keeping this window on top") : tr("Keep this window on top"));
    m_pin->update();
}
bool WorkspaceGroupHeader::pinned() const { return m_pin->isChecked(); }
void WorkspaceGroupHeader::setArrangeAccent(bool on)
{
    if (m_arrange == on) {
        return;
    }
    m_arrange = on;
    applyStyle();
}
QWidget* WorkspaceGroupHeader::grip() const { return m_grip; }
void WorkspaceGroupHeader::setKeepShown(bool keepShown)
{
    m_keepShown->setChecked(keepShown);
    // The tooltip says what a click will do.
    m_keepShown->setToolTip(keepShown ? tr("Hide this header when not arranging")
                                      : tr("Keep this header showing"));
    m_keepShown->update();
}
bool WorkspaceGroupHeader::keepShown() const { return m_keepShown->isChecked(); }

// ---- WorkspaceEntryHeader ---------------------------------------------------

WorkspaceEntryHeader::WorkspaceEntryHeader(const QString& entryId, QWidget* parent)
    : QWidget(parent), m_entryId(entryId)
{
    setObjectName(QStringLiteral("workspaceEntryHeader_") + entryId);
    setFixedHeight(kHeight);
    setMouseTracking(true);
    setCursor(Qt::OpenHandCursor);
    setToolTip(tr("Drag to arrange; right-click for Move, Pop out and Return"));

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(2, 0, 4, 1);
    row->setSpacing(4);

    m_grip = makeGrip(kEntryGripPx, Workspace::kTextInactive, Workspace::kTextSecondary,
                      kEntryGripWidth, kHeight - 1, this);
    row->addWidget(m_grip);

    m_title = new QLabel(this);
    m_title->setFont(Workspace::capsFont(font(), Workspace::kFontCaption));
    m_title->setStyleSheet(QStringLiteral("QLabel { color: %1; background: transparent; }")
                               .arg(Workspace::role("text-scale", Workspace::kTextScale)));
    row->addWidget(m_title, 1);

    // The same action as the entry menu's Pop Out, one click away.
    m_popOut = makeButton(QStringLiteral("↗"), kEntryButtonWidth, kEntryButtonHeight,
                          tr("Pop out"), this);
    m_popOut->setObjectName(QStringLiteral("workspaceEntryPopOut"));
    connect(m_popOut, &QPushButton::clicked, this, &WorkspaceEntryHeader::popOutClicked);
    row->addWidget(m_popOut);

    m_options = makeButton(QStringLiteral("⋯"), kEntryButtonWidth, kEntryButtonHeight,
                           tr("Entry options"), this);
    connect(m_options, &QPushButton::clicked, this, &WorkspaceEntryHeader::optionsClicked);
    row->addWidget(m_options);

    // From Longpath src/gui/applets/GridCellWidget.cpp:170-180 [@551576e3]
    // (the same cross, last and rightmost). NereusSDR: at the entry
    // header's button size; it comes back from Containers > Hidden.
    m_hide = makeButton(QStringLiteral("\u2715"), kEntryButtonWidth, kEntryButtonHeight,
                        tr("Hide. Bring it back from the Containers menu."), this);
    m_hide->setObjectName(QStringLiteral("workspaceEntryHide"));
    connect(m_hide, &QPushButton::clicked, this, &WorkspaceEntryHeader::hideClicked);
    row->addWidget(m_hide);
}

void WorkspaceEntryHeader::setActionsEnabled(bool enabled)
{
    m_popOut->setEnabled(enabled);
    m_hide->setEnabled(enabled);
}

void WorkspaceEntryHeader::setTitle(const QString& title) { m_title->setText(title); }
QString WorkspaceEntryHeader::title() const { return m_title->text(); }
void WorkspaceEntryHeader::setOverlay(bool overlay)
{
    if (m_overlay == overlay) {
        return;
    }
    m_overlay = overlay;
    update();
}

void WorkspaceEntryHeader::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    if (m_overlay) {
        QColor plate(Workspace::role("panel", Workspace::kPanelBg));
        plate.setAlpha(kOverlayPlateAlpha);
        p.fillRect(rect(), plate);
    }
    // Bottom hairline.
    p.fillRect(QRect(0, height() - 1, width(), 1),
               QColor(Workspace::role("border-subtle", Workspace::kBorderSubtle)));
}

} // namespace NereusSDR
