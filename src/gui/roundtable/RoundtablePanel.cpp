// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/roundtable/RoundtablePanel.cpp  (NereusSDR)
// =================================================================
//
// See RoundtablePanel.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 13).
// =================================================================

#include "gui/roundtable/RoundtablePanel.h"

#include "core/AppSettings.h"
#include "core/roundtable/RoundtableWire.h"
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
#include <QTimeZone>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <memory>

namespace NereusSDR {

namespace {

// The workspace roles of the placement mockup (placement.html :root) and
// Roundtable's state colors (design 11, U7, U8). The mockup's 9 and 10 px
// text is 11 px here (U12: never under 11 px).
constexpr const char* kGround = "#0c0c0e";
constexpr const char* kRule = "#1f1f23";
constexpr const char* kText = "#dcdce1";
constexpr const char* kBright = "#e7e7eb";
constexpr const char* kDim = "#a6a6ac";
constexpr const char* kDim2 = "#7e7e85";
constexpr const char* kAccent = "#00b4d8";
constexpr const char* kYou = "#00d4ff";
constexpr const char* kTalking = "#4caf6a";
constexpr const char* kUnknown = "#d8a55f";
constexpr const char* kDoubled = "#c25a5c";
// Design 11.1: a stale row turns grey at about 5:1, not faded.
constexpr const char* kStale = "#84848b";

constexpr int kSmallPx = 11;
constexpr int kBodyPx = 12;
// The readings columns (mockup .rd, with the Age column the design adds).
constexpr int kCallWidth = 84;
constexpr int kKmWidth = 50;
constexpr int kOffsetWidth = 50;
constexpr int kValueWidth = 46;
constexpr int kAgeWidth = 40;
// The rooms the empty state offers to read.
constexpr int kEmptyRooms = 8;
// A message box two lines tall.
constexpr int kBoxHeight = 46;
// S9 is -73 dBm, 6 dB an S-unit (the mockup's sText).
constexpr double kS9Dbm = -73.0;
constexpr double kDbPerSUnit = 6.0;

constexpr const char* kButtonStyle =
    "QPushButton { background: #1a1a1e; border: 1px solid #2c2c31; border-radius: 3px;"
    "  color: #dcdce1; font-size: 11px; padding: 2px 8px; }"
    "QPushButton:hover { border-color: #00b4d8; color: #ffffff; }"
    "QPushButton:disabled { color: #5a5a60; border-color: #232327; }";
constexpr const char* kPrimaryStyle =
    "QPushButton { background: #2f5f92; border: 1px solid #00b4d8; border-radius: 3px;"
    "  color: #ffffff; font-size: 11px; font-weight: bold; padding: 2px 10px; }"
    "QPushButton:hover { background: #3a71ad; }"
    "QPushButton:disabled { background: #141417; border-color: #48484b; color: #5a5a60; }";
constexpr const char* kLinkStyle =
    "QPushButton { background: transparent; border: none; color: #00b4d8; font-size: 11px;"
    "  padding: 0 2px; }"
    "QPushButton:hover { color: #ffffff; }";
constexpr const char* kPillStyle =
    "QPushButton { background: #1e3d5f; border: 1px solid #00b4d8; border-radius: 9px;"
    "  color: #ffffff; font-size: 11px; font-weight: bold; padding: 1px 10px; }"
    "QPushButton:hover { background: #2f5f92; }";
constexpr const char* kMentionPillStyle =
    "QPushButton { background: #00b4d8; border: 1px solid #00d4ff; border-radius: 9px;"
    "  color: #001018; font-size: 11px; font-weight: bold; padding: 1px 10px; }";
constexpr const char* kIconStyle =
    "QToolButton { background: transparent; border: 1px solid transparent; border-radius: 3px;"
    "  color: #a6a6ac; font-size: 12px; padding: 0 4px; }"
    "QToolButton:hover { color: #ffffff; border-color: #2c2c31; }"
    "QToolButton::menu-indicator { image: none; width: 0; }";
constexpr const char* kInputStyle =
    "QLineEdit, QPlainTextEdit { background: #0f0f12; border: 1px solid #3f3f42;"
    "  border-radius: 3px; color: #dcdce1; font-size: 12px; padding: 2px 5px; }"
    "QLineEdit:focus, QPlainTextEdit:focus { border-color: #00b4d8; }"
    "QLineEdit:disabled, QPlainTextEdit:disabled { color: #6a6a70; border-color: #2a2a2e; }";

QString mono()
{
    static const QString family = QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    return family;
}

QString labelStyle(const char* color, int px, bool bold, bool isMono)
{
    QString style = QStringLiteral("color: %1; font-size: %2px; background: transparent;")
                        .arg(QLatin1String(color))
                        .arg(px);
    if (bold) {
        style += QStringLiteral(" font-weight: bold;");
    }
    if (isMono) {
        style += QStringLiteral(" font-family: \"%1\";").arg(mono());
    }
    return style;
}

QLabel* makeLabel(QWidget* parent, const QString& text, const char* color, int px,
                  bool bold = false, bool isMono = false)
{
    auto* label = new QLabel(text, parent);
    label->setStyleSheet(labelStyle(color, px, bold, isMono));
    return label;
}

QLabel* capLabel(QWidget* parent, const QString& text)
{
    QLabel* label = makeLabel(parent, text, kDim, kSmallPx, true);
    QFont font = label->font();
    font.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    label->setFont(font);
    return label;
}

QPushButton* makeButton(QWidget* parent, const QString& text, const char* style)
{
    auto* button = new QPushButton(text, parent);
    button->setStyleSheet(QLatin1String(style));
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
    return QStringLiteral("border-bottom: 1px solid %1;").arg(QLatin1String(kRule));
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
    return QDateTime::fromMSecsSinceEpoch(unixMs, QTimeZone::UTC).toString(QStringLiteral("HH:mm"));
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
    return dash > 0 ? roomId.left(dash) : QString();
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
    html.replace(link, QStringLiteral("<a href=\"\\1\" style=\"color: #00b4d8;\">\\1</a>"));
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
        setMinimumWidth(24);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        const QRect frame = rect().adjusted(0, 0, -1, -1);
        p.fillRect(frame, QColor(0x16, 0x16, 0x19));
        p.setPen(QColor(0x29, 0x29, 0x2c));
        p.drawRect(frame);
        const int width = static_cast<int>(std::lround(m_fraction * (frame.width() - 1)));
        const QRect fill(frame.left() + 1, frame.top() + 1, width, frame.height() - 1);
        if (m_stale) {
            p.fillRect(fill, QColor(0x55, 0x55, 0x5a));
        } else if (m_doubled) {
            p.fillRect(fill, QColor(0x4a, 0x22, 0x24));
            p.setPen(QPen(QColor(0xc2, 0x5a, 0x5c), 2));
            p.setClipRect(fill);
            for (int x = fill.left() - fill.height(); x < fill.right(); x += 6) {
                p.drawLine(x, fill.bottom(), x + fill.height(), fill.top());
            }
        } else {
            QLinearGradient gradient(fill.topLeft(), fill.topRight());
            gradient.setColorAt(0.0, QColor(0x2f, 0x7a, 0x4a));
            gradient.setColorAt(1.0, QColor(0x4c, 0xaf, 0x6a));
            p.fillRect(fill, gradient);
        }
    }

private:
    double m_fraction;
    bool m_doubled;
    bool m_stale;
};

QString messageRowStyle(bool isNew, bool mention)
{
    const char* edge = mention ? kYou : (isNew ? kAccent : "transparent");
    return QStringLiteral("QFrame#roundtableMessageRow { border: none; border-left: 2px solid %1;"
                          "  border-bottom: 1px solid #1c1c1f; background: transparent; }"
                          "QFrame#roundtableMessageRow:focus { background: #16222c; }")
        .arg(QLatin1String(edge));
}

constexpr const char* kTurnRowStyle =
    "QFrame#roundtableTurnRow { border: none; border-bottom: 1px solid #1c1c1f;"
    "  background: transparent; }"
    "QFrame#roundtableTurnRow:focus { background: #16222c; }";

} // namespace

RoundtablePanel::RoundtablePanel(RoundtableModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    setObjectName(QStringLiteral("RoundtablePanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral("QWidget#RoundtablePanel { background: %1; }").arg(QLatin1String(kGround)));
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
    m_scroll->setStyleSheet(QStringLiteral("QScrollArea { background: %1; border: none; }")
                                .arg(QLatin1String(kGround)));
    mainLayout->addWidget(m_scroll, 1);

    m_content = new QWidget();
    m_content->setObjectName(QStringLiteral("roundtableContent"));
    m_content->setAttribute(Qt::WA_StyledBackground, true);
    m_content->setStyleSheet(QStringLiteral("QWidget#roundtableContent { background: %1; }")
                                 .arg(QLatin1String(kGround)));
    m_contentLayout = new QVBoxLayout(m_content);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(0);

    // The read bar (design 11.1).
    m_readBar = new QWidget(m_content);
    m_readBar->setObjectName(QStringLiteral("roundtableReadBar"));
    m_readBar->setAttribute(Qt::WA_StyledBackground, true);
    m_readBar->setStyleSheet(QStringLiteral("QWidget#roundtableReadBar { background: #151518; %1 }")
                                 .arg(rule()));
    {
        auto* row = new QHBoxLayout(m_readBar);
        row->setContentsMargins(8, 4, 8, 4);
        row->setSpacing(6);
        m_readLabel = makeLabel(m_readBar, QString(), kText, kSmallPx, true);
        m_readLabel->setObjectName(QStringLiteral("roundtableReadLabel"));
        row->addWidget(m_readLabel);
        QPushButton* tune = makeButton(m_readBar, QStringLiteral("Tune A here"), kPrimaryStyle);
        tune->setObjectName(QStringLiteral("roundtableTuneHereBtn"));
        tune->setToolTip(QStringLiteral("Tune the slice to this room"));
        connect(tune, &QPushButton::clicked, this, [this]() {
            const QString id = roomId();
            if (!id.isEmpty()) {
                emit tuneRequested(roomHz(id), roomFamily(id));
            }
        });
        row->addWidget(tune);
        row->addStretch(1);
        QPushButton* back = makeButton(m_readBar, QStringLiteral("Back to my slice"), kLinkStyle);
        back->setObjectName(QStringLiteral("roundtableBackBtn"));
        connect(back, &QPushButton::clicked, this, &RoundtablePanel::backToSlice);
        row->addWidget(back);
    }
    m_contentLayout->addWidget(m_readBar);

    m_head = makeSection(m_content, QStringLiteral("roundtableHeader"));
    m_contentLayout->addWidget(m_head);
    m_connection = makeLabel(m_content, QString(), kDim, kSmallPx);
    m_connection->setObjectName(QStringLiteral("roundtableConnectionLine"));
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
                                  .arg(QLatin1String(kGround)));
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
                                   .arg(QLatin1String(kGround), QLatin1String(kRule)));
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
                                .arg(QLatin1String(kGround), QLatin1String(kRule)));
    {
        auto* foot = new QVBoxLayout(m_footer);
        foot->setContentsMargins(8, 5, 8, 5);
        foot->setSpacing(4);
        auto* pills = new QHBoxLayout();
        pills->setSpacing(6);
        m_mentionPill = makeButton(m_footer, QString(), kMentionPillStyle);
        m_mentionPill->setObjectName(QStringLiteral("roundtableMentionPill"));
        connect(m_mentionPill, &QPushButton::clicked, this, &RoundtablePanel::jumpToMention);
        pills->addWidget(m_mentionPill);
        m_newPill = makeButton(m_footer, QString(), kPillStyle);
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
        QLabel* notSentLabel = makeLabel(m_notSent, QStringLiteral("Not sent \u00b7"), kUnknown,
                                         kSmallPx, true);
        notSentLabel->setObjectName(QStringLiteral("roundtableNotSentLabel"));
        notSentRow->addWidget(notSentLabel);
        QPushButton* again = makeButton(m_notSent, QStringLiteral("Try again"), kLinkStyle);
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
        m_box->setStyleSheet(QLatin1String(kInputStyle));
        m_box->setFixedHeight(kBoxHeight);
        m_box->setTabChangesFocus(true);
        m_box->installEventFilter(this);
        connect(m_box, &QPlainTextEdit::textChanged, this, [this]() {
            if (!m_settingBox && m_model && !m_shownId.isEmpty()) {
                m_model->setDraft(m_shownId, m_box->toPlainText());
            }
            updateFooter();
        });
        boxRow->addWidget(m_box, 1);
        m_send = makeButton(m_footer, QStringLiteral("Send"), kPrimaryStyle);
        m_send->setObjectName(QStringLiteral("roundtableSendBtn"));
        connect(m_send, &QPushButton::clicked, this, &RoundtablePanel::send);
        boxRow->addWidget(m_send, 0, Qt::AlignBottom);
        foot->addLayout(boxRow);

        auto* hintRow = new QHBoxLayout();
        hintRow->setSpacing(6);
        m_hint = makeLabel(m_footer, QString(), kDim2, kSmallPx);
        m_hint->setObjectName(QStringLiteral("roundtableHint"));
        m_hint->setWordWrap(true);
        hintRow->addWidget(m_hint, 1);
        m_counter = makeLabel(m_footer, QString(), kDim, kSmallPx, false, true);
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
        connect(m_model, &RoundtableModel::linkChanged, this, &RoundtablePanel::rebuild);
        connect(m_model, &RoundtableModel::sliceRoomsChanged, this, &RoundtablePanel::rebuild);
        connect(m_model, &RoundtableModel::mentionsChanged, this, &RoundtablePanel::rebuild);
        connect(m_model, &RoundtableModel::settingChanged, this, &RoundtablePanel::rebuild);
        connect(m_model, &RoundtableModel::readingRoomChanged, this, [this]() {
            // The Core refused the read, or the room closed.
            if (!m_readId.isEmpty() && m_model->readingRoomId() != m_readId) {
                m_readId.clear();
            }
            rebuild();
        });
        connect(m_model, &RoundtableModel::postResult, this, &RoundtablePanel::onPostResult);
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
    if (hz == m_sliceHz) {
        return;
    }
    m_sliceHz = hz;
    if (roomId().isEmpty()) {
        rebuild();
    }
}

void RoundtablePanel::readRoom(const QString& roomId)
{
    if (!m_model) {
        return;
    }
    const QString mine = m_model->sliceRoomId(m_sliceId);
    if (roomId.isEmpty() || roomId == mine) {
        backToSlice();
        return;
    }
    m_readId = roomId;
    m_model->readRoom(roomId);
    rebuild();
}

void RoundtablePanel::backToSlice()
{
    if (m_readId.isEmpty()) {
        return;
    }
    m_readId.clear();
    if (m_model) {
        m_model->unreadRoom();
    }
    rebuild();
}

QString RoundtablePanel::roomId() const
{
    if (!m_readId.isEmpty()) {
        return m_readId;
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
            return member.value(QStringLiteral("ref")).toString();
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
    return !ref.isEmpty() && message.value(QStringLiteral("ref")).toString() == ref;
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
    // The names given to turns in view, then the room's callsigns.
    QStringList out;
    const QString mine = myCallsign(room);
    const auto add = [&out, &mine](const QString& name) {
        const QString upper = name.trimmed().toUpper();
        if (!upper.isEmpty() && upper != mine && !out.contains(upper)) {
            out.append(upper);
        }
    };
    for (const QString& key : {QStringLiteral("now"), QStringLiteral("earlier")}) {
        for (const QJsonValue& turn : room.value(key).toArray()) {
            for (const QJsonValue& name : turn.toObject().value(QStringLiteral("names")).toArray()) {
                add(name.toObject().value(QStringLiteral("name")).toString());
            }
            for (const QJsonValue& talker : turn.toObject().value(QStringLiteral("talkers")).toArray()) {
                add(talker.toObject().value(QStringLiteral("callsign")).toString());
            }
        }
    }
    for (const QJsonValue& member : room.value(QStringLiteral("members")).toArray()) {
        add(member.toObject().value(QStringLiteral("callsign")).toString());
    }
    while (out.size() > 6) {
        out.removeLast();
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
    if (changed == roomId()) {
        rebuild();
    }
}

void RoundtablePanel::rebuild()
{
    const QString id = roomId();
    if (id != m_shownId) {
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
        m_box->setPlainText(m_model && !id.isEmpty() ? m_model->draft(id) : QString());
        m_settingBox = false;
        emit shownRoomChanged(id);
    }

    m_readBar->setVisible(isReading());
    m_readLabel->setText(QStringLiteral("Reading %1").arg(shortFrequency(roomHz(id))));

    if (id.isEmpty()) {
        for (QWidget* section : {m_head, m_txLine, m_talker, m_readings, m_earlier, m_messages}) {
            section->hide();
        }
        m_empty->show();
        rebuildEmpty();
        updateConnectionLine({});
        m_connection->show();
        updateFooter();
        return;
    }
    m_empty->hide();

    const QJsonObject room = m_model ? m_model->room(id) : QJsonObject();
    const QJsonObject turn = openTurn(room);
    const bool keyed = turn.value(QStringLiteral("mine")).toBool();

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
    auto* row = new QHBoxLayout(body);
    row->setContentsMargins(8, 6, 8, 4);
    row->setSpacing(6);

    const QChar letter(QLatin1Char(static_cast<char>('A' + std::clamp(m_sliceId, 0, 25))));
    QLabel* tag = nullptr;
    if (tunedHere()) {
        tag = makeLabel(body, QString(letter), "#001018", kSmallPx, true);
        tag->setStyleSheet(tag->styleSheet()
                           + QStringLiteral(" background: %1; padding: 0 4px; border-radius: 2px;")
                                 .arg(QLatin1String(kAccent)));
    } else {
        tag = makeLabel(body, QStringLiteral("NOT TUNED HERE"), kUnknown, kSmallPx, true);
        tag->setStyleSheet(tag->styleSheet()
                           + QStringLiteral(" border: 1px solid #6b5426; padding: 0 4px;"
                                            " border-radius: 2px;"));
    }
    tag->setObjectName(QStringLiteral("roundtableTag"));
    row->addWidget(tag);
    QLabel* freq = makeLabel(body, headerFrequency(roomHz(id)), kAccent, 15, false, true);
    freq->setObjectName(QStringLiteral("roundtableFrequency"));
    row->addWidget(freq);
    QLabel* mode = makeLabel(body, modeLabel(roomFamily(id)), "#c3c3c7", kSmallPx);
    mode->setStyleSheet(mode->styleSheet()
                        + QStringLiteral(" border: 1px solid #48484b; padding: 0 4px; border-radius: 2px;"));
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
    QString countText = QStringLiteral("%1 in the room").arg(count);
    if (away > 0) {
        countText += QStringLiteral(" \u00b7 %1 away").arg(away);
    }
    QLabel* countLabel = makeLabel(body, countText, kDim, kSmallPx);
    countLabel->setObjectName(QStringLiteral("roundtableCount"));
    row->addWidget(countLabel);

    auto* pop = new QToolButton(body);
    pop->setObjectName(QStringLiteral("roundtablePopBtn"));
    pop->setText(QString(QChar(0x2197)));
    pop->setToolTip(QStringLiteral("Pop out into its own window"));
    pop->setStyleSheet(QLatin1String(kIconStyle));
    pop->setFocusPolicy(Qt::ClickFocus);
    pop->setVisible(!m_popped);
    connect(pop, &QToolButton::clicked, this, &RoundtablePanel::popOutRequested);
    row->addWidget(pop);
    m_popButton = pop;

    auto* menu = new QToolButton(body);
    menu->setObjectName(QStringLiteral("roundtableMenuBtn"));
    menu->setText(QStringLiteral("\u22ef"));
    menu->setToolTip(m_popped ? QStringLiteral("Archive, settings and the wide window")
                              : QStringLiteral("Archive, settings and pop out"));
    menu->setStyleSheet(QLatin1String(kIconStyle));
    menu->setFocusPolicy(Qt::ClickFocus);
    connect(menu, &QToolButton::clicked, this, &RoundtablePanel::showPanelMenu);
    row->addWidget(menu);
}

void RoundtablePanel::updateConnectionLine(const QJsonObject& room)
{
    using LinkState = RoundtableModel::LinkState;
    const qint64 now = RoundtableModel::nowMs();
    const LinkState state = m_model ? m_model->linkState() : LinkState::Off;
    const qint64 since = m_model ? m_model->linkSince() : 0;
    QString text;
    const char* color = kDim;
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
        color = kTalking;
        break;
    }
    case LinkState::Reconnecting:
        text = QStringLiteral("Reconnecting \u00b7 nothing new for %1").arg(ageText(now - since));
        color = kUnknown;
        break;
    case LinkState::Offline:
        text = since > 0 ? QStringLiteral("Offline since %1 UTC").arg(utcText(since))
                         : QStringLiteral("Offline");
        color = kUnknown;
        break;
    case LinkState::Connecting:
        text = QStringLiteral("Connecting");
        break;
    case LinkState::Hidden:
        text = QStringLiteral("You are hidden");
        color = kYou;
        break;
    case LinkState::Off:
        text = QStringLiteral("Roundtable is off");
        break;
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
    const bool keyed = turn.value(QStringLiteral("mine")).toBool();
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
        QLabel* tx = makeLabel(body, QStringLiteral("TX"), "#ffffff", kSmallPx, true);
        tx->setStyleSheet(tx->styleSheet() + QStringLiteral(" background: #c02020; padding: 0 4px;"
                                                            " border-radius: 2px;"));
        row->addWidget(tx);
        QLabel* who = makeLabel(body, QStringLiteral("You \u00b7 %1 hear you").arg(hearing), kYou,
                                kBodyPx, true);
        who->setObjectName(QStringLiteral("roundtableTxText"));
        row->addWidget(who);
        QLabel* time = makeLabel(body, keyedText, kText, kBodyPx, false, true);
        time->setObjectName(QStringLiteral("roundtableKeyedTime"));
        row->addWidget(time);
        row->addStretch(1);
        row->addWidget(makeLabel(body, headerFrequency(roomHz(roomId())), kAccent, kBodyPx, false, true));
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
                                 kDim, kBodyPx));
        return;
    }

    const auto gridOf = [&room](const QString& ref) {
        for (const QJsonValue& value : room.value(QStringLiteral("members")).toArray()) {
            if (value.toObject().value(QStringLiteral("ref")).toString() == ref) {
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
        QLabel* tx = makeLabel(body, QStringLiteral("TX"), "#ffffff", kSmallPx, true);
        tx->setStyleSheet(tx->styleSheet() + QStringLiteral(" background: #c03030; padding: 0 4px;"
                                                            " border-radius: 2px;"));
        return tx;
    };
    const QJsonArray talkers = turn.value(QStringLiteral("talkers")).toArray();

    if (turn.isEmpty()) {
        col->addWidget(capLabel(body, QStringLiteral("QUIET")));
        QLabel* big = makeLabel(body, QStringLiteral("Nobody is talking"), kDim, 14, true, true);
        big->setObjectName(QStringLiteral("roundtableTalkerName"));
        col->addWidget(big);
        if (!earlier.isEmpty()) {
            const QJsonObject last = earlier.first().toObject();
            const qint64 ago = now - last.value(QStringLiteral("end")).toInteger();
            const QJsonArray lastTalkers = last.value(QStringLiteral("talkers")).toArray();
            QString text;
            if (last.value(QStringLiteral("mine")).toBool()) {
                text = QStringLiteral("Last turn was yours, %1 ago").arg(ageText(ago));
            } else if (!lastTalkers.isEmpty()
                       && last.value(QStringLiteral("kind")).toString() != QLatin1String("unknown")) {
                text = QStringLiteral("Last turn by %1, %2 ago")
                           .arg(withNth(lastTalkers.first().toObject()), ageText(ago));
            } else {
                text = QStringLiteral("Last turn by an unknown talker, %1 ago").arg(ageText(ago));
            }
            col->addWidget(makeLabel(body, text, kDim, kSmallPx));
        }
        return;
    }

    if (kind == QLatin1String("doubled")) {
        col->addWidget(capLabel(body, QStringLiteral("TWO AT ONCE")));
        QHBoxLayout* row = bigRow();
        for (qsizetype i = 0; i < talkers.size(); ++i) {
            if (i > 0) {
                row->addWidget(makeLabel(body, QStringLiteral("+"), kDim, 17, true, true));
            }
            row->addWidget(txBadge());
            QLabel* name = makeLabel(body, withNth(talkers.at(i).toObject()), kDoubled, 17, true, true);
            name->setObjectName(QStringLiteral("roundtableTalkerName"));
            row->addWidget(name);
        }
        QLabel* tag = makeLabel(body, QStringLiteral("DOUBLED"), "#f0dcdc", kSmallPx, true);
        tag->setStyleSheet(tag->styleSheet() + QStringLiteral(" background: #5a1616; border: 1px solid"
                                                              " #c25a5c; padding: 0 4px;"));
        row->addWidget(tag);
        row->addStretch(1);
        auto* sub = new QHBoxLayout();
        sub->setSpacing(4);
        sub->addWidget(makeLabel(body, QStringLiteral("for"), kDim, kSmallPx));
        QLabel* time = makeLabel(body, keyedText, kDim, kSmallPx, false, true);
        time->setObjectName(QStringLiteral("roundtableKeyedTime"));
        sub->addWidget(time);
        sub->addWidget(makeLabel(body, QStringLiteral("\u00b7 each reading is both signals together"),
                                 kDim, kSmallPx));
        sub->addStretch(1);
        col->addLayout(sub);
        return;
    }

    if (kind == QLatin1String("unknown")) {
        col->addWidget(capLabel(body, QStringLiteral("TALKING NOW")));
        QLabel* big = makeLabel(body, QStringLiteral("? Unknown"), kUnknown, 17, true, true);
        big->setObjectName(QStringLiteral("roundtableTalkerName"));
        col->addWidget(big);
        auto* sub = new QHBoxLayout();
        sub->setSpacing(4);
        sub->addWidget(makeLabel(body, QStringLiteral("not on NereusSDR \u00b7 on air"), kDim, kSmallPx));
        QLabel* time = makeLabel(body, keyedText, kDim, kSmallPx, false, true);
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
            QLabel* named = makeLabel(body, QStringLiteral("Named %1").arg(given.join(QStringLiteral(", "))),
                                      kUnknown, kSmallPx);
            named->setObjectName(QStringLiteral("roundtableNamesGiven"));
            col->addWidget(named);
        }
        // NAME THIS TURN: the names in view as chips, and a callsign box.
        auto* names = new QHBoxLayout();
        names->setSpacing(4);
        names->addWidget(capLabel(body, QStringLiteral("NAME THIS TURN")));
        const qint64 turnId = turn.value(QStringLiteral("id")).toInteger();
        for (const QString& suggestion : nameSuggestions(room)) {
            QPushButton* chip = makeButton(body, suggestion, kButtonStyle);
            chip->setObjectName(QStringLiteral("roundtableNameChip"));
            chip->setEnabled(canName);
            chip->setToolTip(canName ? QStringLiteral("Name this turn %1").arg(suggestion)
                                     : QStringLiteral("Tune a slice here to name a turn"));
            connect(chip, &QPushButton::clicked, this,
                    [this, turnId, suggestion]() { submitName(turnId, suggestion); });
            names->addWidget(chip);
        }
        names->addStretch(1);
        col->addLayout(names);
        auto* inputRow = new QHBoxLayout();
        inputRow->setSpacing(4);
        auto* input = new QLineEdit(body);
        input->setObjectName(QStringLiteral("roundtableTalkerNameInput"));
        input->setPlaceholderText(QStringLiteral("Callsign"));
        input->setStyleSheet(QLatin1String(kInputStyle));
        input->setMaxLength(16);
        input->setEnabled(canName);
        input->setFixedWidth(110);
        inputRow->addWidget(input);
        QPushButton* nameButton = makeButton(body, QStringLiteral("Name"), kButtonStyle);
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
    QLabel* name = makeLabel(body, withNth(talker), kTalking, 17, true, true);
    name->setObjectName(QStringLiteral("roundtableTalkerName"));
    row->addWidget(name);
    row->addWidget(makeLabel(body, gridOf(talker.value(QStringLiteral("ref")).toString()), kDim,
                             kSmallPx));
    row->addStretch(1);
    auto* sub = new QHBoxLayout();
    sub->setSpacing(4);
    sub->addWidget(makeLabel(body, QStringLiteral("keyed"), kDim, kSmallPx));
    QLabel* time = makeLabel(body, keyedText, kDim, kSmallPx, false, true);
    time->setObjectName(QStringLiteral("roundtableKeyedTime"));
    sub->addWidget(time);
    sub->addWidget(makeLabel(body, QStringLiteral("\u00b7 on NereusSDR, so the talker is known"),
                             kDim, kSmallPx));
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
    const QJsonObject turn = openTurn(room);
    const QString kind = turn.value(QStringLiteral("kind")).toString();
    const bool keyed = turn.value(QStringLiteral("mine")).toBool();
    QSet<QString> talkerRefs;
    QStringList talkerNames;
    for (const QJsonValue& value : turn.value(QStringLiteral("talkers")).toArray()) {
        talkerRefs.insert(value.toObject().value(QStringLiteral("ref")).toString());
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
        if (talkerRefs.contains(member.value(QStringLiteral("ref")).toString()) || (keyed && isYou)
            || !member.value(QStringLiteral("reading")).isObject()) {
            continue;
        }
        Row row;
        row.member = member;
        const QJsonValue km = member.value(QStringLiteral("km"));
        row.hasKm = km.isDouble();
        row.km = km.toDouble();
        rows.append(row);
    }
    // Nearest first; rows with no distance follow by callsign (6.8).
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        if (a.hasKm != b.hasKm) {
            return a.hasKm;
        }
        if (a.hasKm && a.km != b.km) {
            return a.km < b.km;
        }
        return a.member.value(QStringLiteral("callsign")).toString()
             < b.member.value(QStringLiteral("callsign")).toString();
    });
    const bool byDistance = !rows.isEmpty() && rows.first().hasKm;

    auto* head = new QHBoxLayout();
    head->setContentsMargins(8, 6, 8, 3);
    QLabel* titleLabel = capLabel(body, title);
    titleLabel->setObjectName(QStringLiteral("roundtableReadingsTitle"));
    head->addWidget(titleLabel, 1);
    if (!turn.isEmpty() && !rows.isEmpty()) {
        head->addWidget(makeLabel(body, byDistance ? QStringLiteral("nearest first")
                                                   : QStringLiteral("by callsign"),
                                  kDim2, kSmallPx));
    }
    col->addLayout(head);

    if (others.isEmpty()) {
        QLabel* lonely = makeLabel(body, QStringLiteral("Nobody else is here. Your readings of the next "
                                                        "talker will appear here."),
                                   kDim, kSmallPx);
        lonely->setObjectName(QStringLiteral("roundtableNobodyElse"));
        lonely->setWordWrap(true);
        lonely->setContentsMargins(8, 2, 8, 6);
        col->addWidget(lonely);
        return;
    }
    if (turn.isEmpty()) {
        // Quiet: who is in the room.
        QLabel* list = makeLabel(body, others.join(QStringLiteral("  ")), kText, kSmallPx, false, true);
        list->setObjectName(QStringLiteral("roundtableInTheRoom"));
        list->setWordWrap(true);
        list->setContentsMargins(8, 2, 8, 6);
        col->addWidget(list);
        return;
    }

    const auto columns = [](QHBoxLayout* row) {
        row->setContentsMargins(8, 0, 8, 0);
        row->setSpacing(4);
    };
    {
        auto* header = new QHBoxLayout();
        columns(header);
        const auto cell = [body](const QString& text, int width, Qt::Alignment align) {
            QLabel* label = makeLabel(body, text, kDim2, kSmallPx);
            if (width > 0) {
                label->setFixedWidth(width);
            }
            label->setAlignment(align | Qt::AlignVCenter);
            return label;
        };
        header->addWidget(cell(QStringLiteral("Station"), kCallWidth, Qt::AlignLeft));
        header->addWidget(cell(QStringLiteral("km"), kKmWidth, Qt::AlignRight));
        header->addWidget(cell(QStringLiteral("Offset"), kOffsetWidth, Qt::AlignRight));
        header->addWidget(cell(QStringLiteral("Reading"), 0, Qt::AlignLeft), 1);
        header->addWidget(cell(QStringLiteral("Age"), kAgeWidth, Qt::AlignRight));
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
        rowWidget->setFixedHeight(20);
        if (isYou) {
            rowWidget->setStyleSheet(QStringLiteral("QFrame#roundtableReadingRow {"
                                                    " background: rgba(0, 180, 216, 20); }"));
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
        const char* text = stale ? kStale : (isYou ? kYou : (away ? "#959599" : kText));

        auto* callCell = new QWidget(rowWidget);
        callCell->setFixedWidth(kCallWidth);
        auto* callRow = new QHBoxLayout(callCell);
        callRow->setContentsMargins(0, 0, 0, 0);
        callRow->setSpacing(3);
        QLabel* call = makeLabel(callCell, withNth(member), text, kSmallPx, true, true);
        call->setObjectName(QStringLiteral("roundtableReadingCall"));
        callRow->addWidget(call);
        if (away) {
            QLabel* awayTag = makeLabel(callCell, QStringLiteral("AWAY"), "#a5a5a9", kSmallPx, true);
            awayTag->setObjectName(QStringLiteral("roundtableAwayTag"));
            awayTag->setStyleSheet(awayTag->styleSheet()
                                   + QStringLiteral(" border: 1px solid #48484b; border-radius: 2px;"
                                                    " padding: 0 2px;"));
            callRow->addWidget(awayTag);
        }
        callRow->addStretch(1);
        row->addWidget(callCell);

        QLabel* km = makeLabel(rowWidget,
                               entry.hasKm ? QString::number(std::lround(entry.km))
                                           : member.value(QStringLiteral("grid")).toString(),
                               stale ? kStale : kDim, kSmallPx, false, true);
        km->setFixedWidth(kKmWidth);
        km->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(km);
        const qint64 offsetHz = member.value(QStringLiteral("offsetHz")).toInteger();
        QLabel* offset = makeLabel(rowWidget, offsetText(offsetHz),
                                   stale ? kStale : (std::llabs(offsetHz) >= 50 ? kUnknown : kDim),
                                   kSmallPx, false, true);
        offset->setFixedWidth(kOffsetWidth);
        offset->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(offset);
        row->addWidget(new ReadingBar(present ? sFraction(dbm) : 0.0, kind == QLatin1String("doubled"),
                                      stale, rowWidget),
                       1);
        QLabel* value = makeLabel(rowWidget, present ? sText(dbm) : QStringLiteral("none"), text,
                                  kSmallPx, false, true);
        value->setObjectName(QStringLiteral("roundtableReadingValue"));
        value->setFixedWidth(kValueWidth);
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(value);
        // The age turns amber once stale (U4).
        QLabel* ageLabel = makeLabel(rowWidget, ageText(age), stale ? kUnknown : kDim, kSmallPx,
                                     stale, true);
        ageLabel->setObjectName(QStringLiteral("roundtableReadingAge"));
        ageLabel->setFixedWidth(kAgeWidth);
        ageLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(ageLabel);
        col->addWidget(rowWidget);
    }
    if (anyAway) {
        QLabel* note = makeLabel(body, QStringLiteral("AWAY: nobody at the radio. Its readings still "
                                                      "count, but it can't post or name turns."),
                                 kDim, kSmallPx);
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
    head->addWidget(makeLabel(body, QStringLiteral("last 30 minutes"), kDim2, kSmallPx));
    col->addLayout(head);

    QList<QPointer<QWidget>> turnItems;
    QWidget* focusTarget = nullptr;
    for (qsizetype i = 0; i < earlier.size() && i < limit; ++i) {
        const QJsonObject turn = earlier.at(i).toObject();
        const qint64 id = turn.value(QStringLiteral("id")).toInteger();
        const QString kind = turn.value(QStringLiteral("kind")).toString();
        const bool mine = turn.value(QStringLiteral("mine")).toBool();
        auto* row = new QFrame(body);
        row->setObjectName(QStringLiteral("roundtableTurnRow"));
        row->setStyleSheet(QLatin1String(kTurnRowStyle));
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
        line->addWidget(makeLabel(row, utcText(turn.value(QStringLiteral("start")).toInteger()), kDim,
                                  kSmallPx, false, true));
        QStringList talkers;
        for (const QJsonValue& value : turn.value(QStringLiteral("talkers")).toArray()) {
            talkers.append(withNth(value.toObject()));
        }
        QString who;
        const char* color = kTalking;
        if (mine) {
            who = QStringLiteral("You");
            color = kYou;
        } else if (kind == QLatin1String("doubled")) {
            who = talkers.join(QStringLiteral(" + "));
            color = kDoubled;
        } else if (kind == QLatin1String("unknown")) {
            who = QStringLiteral("Unknown");
            color = kUnknown;
        } else {
            who = talkers.isEmpty() ? QStringLiteral("Unknown") : talkers.first();
        }
        QLabel* whoLabel = makeLabel(row, who, color, kSmallPx, true, true);
        whoLabel->setObjectName(QStringLiteral("roundtableTurnWho"));
        line->addWidget(whoLabel);
        QStringList given;
        for (const QJsonValue& value : turn.value(QStringLiteral("names")).toArray()) {
            const QJsonObject name = value.toObject();
            given.append(QStringLiteral("%1 (%2)").arg(name.value(QStringLiteral("name")).toString())
                             .arg(name.value(QStringLiteral("count")).toInt()));
        }
        if (!given.isEmpty()) {
            QLabel* named = makeLabel(row, QStringLiteral("named %1").arg(given.join(QStringLiteral(", "))),
                                      kDim, kSmallPx);
            named->setObjectName(QStringLiteral("roundtableTurnNames"));
            line->addWidget(named);
        }
        line->addStretch(1);
        if (nameable) {
            // U9: name a turn after it ends.
            QPushButton* name = makeButton(row, QStringLiteral("Name"), kButtonStyle);
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
            auto* edit = new QHBoxLayout(editor);
            edit->setContentsMargins(8, 2, 8, 4);
            edit->setSpacing(4);
            for (const QString& suggestion : nameSuggestions(room)) {
                QPushButton* chip = makeButton(editor, suggestion, kButtonStyle);
                chip->setObjectName(QStringLiteral("roundtableNameChip"));
                connect(chip, &QPushButton::clicked, this,
                        [this, id, suggestion]() { submitName(id, suggestion); });
                edit->addWidget(chip);
            }
            auto* input = new QLineEdit(editor);
            input->setObjectName(QStringLiteral("roundtableNameInput"));
            input->setPlaceholderText(QStringLiteral("Callsign"));
            input->setStyleSheet(QLatin1String(kInputStyle));
            input->setMaxLength(16);
            input->setFixedWidth(100);
            input->setProperty("nameEditorFor", id);
            input->installEventFilter(this);
            connect(input, &QLineEdit::returnPressed, this,
                    [this, id, input]() { submitName(id, input->text()); });
            edit->addWidget(input);
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
    const QJsonArray messages = room.value(QStringLiteral("messages")).toArray();
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
                            + myRef(room) + myCallsign(room);
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
    int newCount = 0;
    for (const qint64 id : std::as_const(m_new)) {
        if (!m_everOnScreen.contains(id)) {
            ++newCount;
        }
    }
    QLabel* tally = makeLabel(body, newCount > 0 ? QStringLiteral("%1, %2 new").arg(messages.size()).arg(newCount)
                                                 : QString::number(messages.size()),
                              kDim2, kSmallPx, false, true);
    tally->setObjectName(QStringLiteral("roundtableMessagesTally"));
    head->addWidget(tally);
    col->addLayout(head);

    if (messages.isEmpty()) {
        QLabel* none = makeLabel(body, QStringLiteral("No messages on %1 so far.")
                                           .arg(headerFrequency(roomHz(roomId()))),
                                 kDim, kSmallPx);
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
        row->setProperty("isNew", isNew);
        row->setProperty("mention", mention);
        row->setProperty("unreadMention", unread.contains(id));
        row->installEventFilter(this);
        row->setStyleSheet(messageRowStyle(isNew, mention));
        auto* rowCol = new QVBoxLayout(row);
        rowCol->setContentsMargins(8, 4, 8, 4);
        rowCol->setSpacing(2);
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
                                     kDim2, kSmallPx);
            text->setObjectName(QStringLiteral("roundtableMutedFold"));
            QFont italic = text->font();
            italic.setItalic(true);
            text->setFont(italic);
            fold->addWidget(text);
            QPushButton* show = makeButton(row, QStringLiteral("Show"), kLinkStyle);
            show->setObjectName(QStringLiteral("roundtableShowMutedBtn"));
            connect(show, &QPushButton::clicked, this, [this, id]() {
                m_showMuted.insert(id);
                rebuild();
            });
            fold->addWidget(show);
            QPushButton* unmute = makeButton(row, QStringLiteral("Unmute %1").arg(callsign), kLinkStyle);
            connect(unmute, &QPushButton::clicked, this, [this, callsign]() {
                if (m_model) {
                    m_model->unmute(callsign);
                }
            });
            fold->addWidget(unmute);
            fold->addStretch(1);
            rowCol->addLayout(fold);
            col->addWidget(row);
            continue;
        }

        auto* top = new QHBoxLayout();
        top->setSpacing(6);
        QLabel* who = makeLabel(row, withNth(message), isMine ? kYou : kBright, kSmallPx, true, true);
        who->setObjectName(QStringLiteral("roundtableMessageWho"));
        top->addWidget(who);
        const QJsonValue dbm = message.value(QStringLiteral("dbm"));
        if (dbm.isDouble()) {
            QLabel* chip = makeLabel(row, sText(dbm.toDouble()), "#aeaeb2", kSmallPx, false, true);
            chip->setToolTip(QStringLiteral("Their reading of the talker when they sent it"));
            chip->setStyleSheet(chip->styleSheet()
                                + QStringLiteral(" background: #18181b; border: 1px solid #2b2b2e;"
                                                 " border-radius: 3px; padding: 0 4px;"));
            top->addWidget(chip);
        }
        top->addStretch(1);
        QLabel* fresh = makeLabel(row, QStringLiteral("new"), kAccent, kSmallPx, true);
        fresh->setObjectName(QStringLiteral("roundtableNewMark"));
        fresh->setVisible(isNew);
        top->addWidget(fresh);
        top->addWidget(makeLabel(row, utcText(message.value(QStringLiteral("t")).toInteger()), kDim2,
                                 kSmallPx, false, true));
        if (state == QLatin1String("shown")) {
            auto* more = new QToolButton(row);
            more->setObjectName(QStringLiteral("roundtableMessageMenuBtn"));
            more->setText(QStringLiteral("\u22ef"));
            more->setToolTip(isMine ? QStringLiteral("Delete") : QStringLiteral("Mute, report or copy"));
            more->setStyleSheet(QLatin1String(kIconStyle));
            more->setFocusPolicy(Qt::NoFocus);
            connect(more, &QToolButton::clicked, this,
                    [this, more, message]() { showMessageMenu(more, message); });
            top->addWidget(more);
        }
        rowCol->addLayout(top);

        QLabel* text = nullptr;
        if (state == QLatin1String("removed")) {
            text = makeLabel(row, QStringLiteral("Message removed"), kDim2, kSmallPx);
        } else if (state == QLatin1String("deleted")) {
            text = makeLabel(row, QStringLiteral("Deleted by sender"), kDim2, kSmallPx);
        } else if (state == QLatin1String("expired")) {
            text = makeLabel(row, QStringLiteral("Message removed after a year"), kDim2, kSmallPx);
        } else {
            text = makeLabel(row, QString(), "#e1e1e5", kBodyPx);
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
    QLabel* tag = makeLabel(body, QString(letter), "#001018", kSmallPx, true);
    tag->setStyleSheet(tag->styleSheet()
                       + QStringLiteral(" background: %1; padding: 0 4px; border-radius: 2px;")
                             .arg(QLatin1String(kAccent)));
    head->addWidget(tag);
    if (m_sliceHz > 0) {
        head->addWidget(makeLabel(body, headerFrequency(m_sliceHz), kAccent, 15, false, true));
    }
    head->addStretch(1);
    auto* pop = new QToolButton(body);
    pop->setObjectName(QStringLiteral("roundtablePopBtn"));
    pop->setText(QString(QChar(0x2197)));
    pop->setToolTip(QStringLiteral("Pop out into its own window"));
    pop->setStyleSheet(QLatin1String(kIconStyle));
    pop->setFocusPolicy(Qt::ClickFocus);
    pop->setVisible(!m_popped);
    connect(pop, &QToolButton::clicked, this, &RoundtablePanel::popOutRequested);
    head->addWidget(pop);
    auto* menu = new QToolButton(body);
    menu->setObjectName(QStringLiteral("roundtableMenuBtn"));
    menu->setText(QStringLiteral("\u22ef"));
    menu->setToolTip(m_popped ? QStringLiteral("Archive, settings and the wide window")
                              : QStringLiteral("Archive, settings and pop out"));
    menu->setStyleSheet(QLatin1String(kIconStyle));
    menu->setFocusPolicy(Qt::ClickFocus);
    connect(menu, &QToolButton::clicked, this, &RoundtablePanel::showPanelMenu);
    head->addWidget(menu);
    col->addLayout(head);

    QLabel* title = makeLabel(body, QStringLiteral("Nobody on Roundtable is tuned here."), kBright,
                              kBodyPx, true);
    title->setObjectName(QStringLiteral("roundtableEmptyTitle"));
    title->setContentsMargins(8, 4, 8, 0);
    col->addWidget(title);
    QLabel* how = makeLabel(body, QStringLiteral("Click a callsign on the panadapter to tune there, or "
                                                 "one of these to read it."),
                            kDim, kSmallPx);
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
                                       kButtonStyle);
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
        } else if (turn.value(QStringLiteral("mine")).toBool()) {
            hint = QStringLiteral("Sent as transmitting");
        } else if (turn.value(QStringLiteral("kind")).toString() == QLatin1String("unknown")
                   || talkers.isEmpty()) {
            hint = QStringLiteral("Sent with your reading of the talker");
        } else {
            QStringList who;
            for (const QJsonValue& value : talkers) {
                who.append(withNth(value.toObject()));
            }
            hint = QStringLiteral("Sent with your reading of %1").arg(who.join(QStringLiteral(" + ")));
        }
    }
    m_hint->setText(hint);

    // The counter from 400 code points (design 11.1).
    if (points >= kCounterFrom || lines > kMessageLines) {
        m_counter->setText(lines > kMessageLines
                               ? QStringLiteral("%1 lines, up to %2").arg(lines).arg(kMessageLines)
                               : QStringLiteral("%1 / %2").arg(points).arg(kMessageLimit));
        m_counter->setStyleSheet(labelStyle(fits ? kDim : kUnknown, kSmallPx, !fits, true));
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
    const QString id = roomId();
    if (id.isEmpty()) {
        updateConnectionLine({});
        return;
    }
    const QJsonObject room = m_model ? m_model->room(id) : QJsonObject();
    updateConnectionLine(room);
    rebuildTalker(room);
    rebuildReadings(room);
    checkOnScreen();
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
        const bool onScreen = isVisible() && row->isVisible() && view.intersects(place);
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
    m_pending.insert(clientId, Pending{id, text});
    m_notSent->hide();
    m_settingBox = true;
    m_box->clear();
    m_settingBox = false;
    m_model->setDraft(id, QString());
    updateFooter();
    scrollMessagesToEnd();
    m_model->post(id, text, clientId);
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
    if (m_model && m_model->draft(pending.roomId).isEmpty()) {
        m_model->setDraft(pending.roomId, pending.text);
    }
    if (pending.roomId == m_shownId) {
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
    if (!m_model || callsign.isEmpty() || turnId <= 0) {
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
    const QString target = roomId() + QLatin1Char('/') + member.value(QStringLiteral("ref")).toString();
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
        menu->addAction(QStringLiteral("Pop out"), this, &RoundtablePanel::popOutRequested);
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
