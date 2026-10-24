// no-port-check: NereusSDR-original.
// =================================================================
// src/gui/roundtable/RoundtableFirstRunDialog.cpp  (NereusSDR)
// =================================================================
//
// See RoundtableFirstRunDialog.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 12).
// =================================================================

#include "gui/roundtable/RoundtableFirstRunDialog.h"

#include "core/AppSettings.h"
#include "core/SpotSourceHost.h"
#include "core/roundtable/RoundtableWire.h"
#include "models/RoundtableModel.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace NereusSDR {

namespace {

// The workspace roles the board mockup's first-run sheet uses
// (docs/architecture/reporter-brainstorm/mockups/board.html, .rt-first).
constexpr const char* kSheetStyle =
    "QDialog#RoundtableFirstRunDialog { background: #0c0c0e; }"
    "QLabel { color: #dcdce1; font-size: 13px; }"
    "QLabel#roundtableFirstRunTitle { color: #ececf0; font-size: 18px; font-weight: bold; }"
    "QLabel#roundtableFirstRunFoot { color: #a6a6ac; font-size: 12px; }"
    "QLabel#roundtableFirstRunError { color: #d8a55f; font-size: 12px; }"
    "QFrame#roundtableFirstRunItem { border-top: 1px solid #2a2a2e; }"
    "QFrame#roundtableFirstRunHideRow { background: #111115; border: 1px solid #2c2c31;"
    "  border-radius: 8px; }"
    "QFrame#roundtableFirstRunHideRow QLabel { background: transparent; }"
    "QLabel#roundtableFirstRunHideTitle { color: #ececf0; font-size: 14px; font-weight: bold; }"
    "QLabel#roundtableFirstRunHideText { color: #a6a6ac; font-size: 12px; }"
    "QPushButton#roundtableFirstRunHideSwitch { min-width: 52px; min-height: 26px;"
    "  border-radius: 13px; border: 1px solid #3d3d41; background: #1a1a1e; color: #a6a6ac;"
    "  font-size: 12px; font-weight: bold; }"
    "QPushButton#roundtableFirstRunHideSwitch:checked { background: #1e3d5f;"
    "  border-color: #4a7ba8; color: #ffffff; }";

// The main button and the quiet one (.rt-mbtns .is-go / .is-quiet).
constexpr const char* kMainButtonStyle =
    "QPushButton { min-height: 44px; border-radius: 10px; font-size: 14px; font-weight: bold;"
    "  background: #1e3d5f; border: 1px solid #4a7ba8; color: #ffffff; padding: 0 16px; }"
    "QPushButton:hover { background: #2f5f92; }"
    "QPushButton:disabled { background: #141417; border-color: #232327; color: #4e4e53; }";
constexpr const char* kQuietButtonStyle =
    "QPushButton { min-height: 44px; border-radius: 10px; font-size: 14px; font-weight: bold;"
    "  background: transparent; border: 1px solid #2c2c31; color: #b4b4b8; padding: 0 16px; }"
    "QPushButton:hover { background: #1a1a1e; }"
    "QPushButton:disabled { background: #141417; border-color: #232327; color: #4e4e53; }";

// The state glyphs of design 11 (U7): you a filled cyan dot, talking a
// filled green dot, unknown a ringed amber dot, quiet a hollow ring.
QLabel* glyph(const char* fill, const char* ring)
{
    auto* dot = new QLabel;
    dot->setFixedSize(12, 12);
    dot->setStyleSheet(QStringLiteral("QLabel { background: %1; border: 2px solid %2;"
                                      " border-radius: 6px; }")
                           .arg(QLatin1String(fill), QLatin1String(ring)));
    return dot;
}

QFrame* item(QLabel* dot, const QString& html)
{
    auto* row = new QFrame;
    row->setObjectName(QStringLiteral("roundtableFirstRunItem"));
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 7, 0, 7);
    layout->setSpacing(10);
    layout->addWidget(dot, 0, Qt::AlignVCenter);
    auto* text = new QLabel(html);
    text->setTextFormat(Qt::RichText);
    text->setWordWrap(true);
    layout->addWidget(text, 1);
    return row;
}

QString youMark(const QString& text)
{
    return QStringLiteral("<b style='color:#00d4ff; font-family:Menlo,monospace;'>%1</b>")
        .arg(text.toHtmlEscaped());
}

} // namespace

bool RoundtableFirstRunDialog::shouldShow(const RoundtableModel* model)
{
    if (model == nullptr || !model->available()) {
        return false;
    }
    if (!model->firstRunAnswered()) {
        return true;
    }
    return AppSettings::instance().value(QLatin1String(kSeenKey)).toString()
        != QLatin1String("True");
}

RoundtableFirstRunDialog::RoundtableFirstRunDialog(RoundtableModel* model, QWidget* parent)
    : QDialog(parent)
    , m_model(model)
{
    setObjectName(QStringLiteral("RoundtableFirstRunDialog"));
    setWindowTitle(QStringLiteral("Roundtable is public"));
    setStyleSheet(QLatin1String(kSheetStyle));
    setMinimumWidth(480);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 16);
    // Two pages, one shown: a hidden page takes no room, so the note
    // dialog fits its own words.
    m_question = new QWidget;
    buildQuestion(m_question);
    root->addWidget(m_question);
    m_note = new QWidget;
    buildNote(m_note);
    m_note->hide();
    root->addWidget(m_note);

    if (m_model) {
        connect(m_model, &RoundtableModel::firstRunChanged, this,
                &RoundtableFirstRunDialog::onFirstRunChanged);
        connect(m_model, &RoundtableModel::actionRefused, this,
                &RoundtableFirstRunDialog::onRefused);
    }
    if (m_model && m_model->firstRunAnswered()) {
        showNote();
    } else {
        // Asked here: a later answer from another device needs no note.
        markSeen();
    }
}

RoundtableFirstRunDialog::~RoundtableFirstRunDialog() = default;

void RoundtableFirstRunDialog::buildQuestion(QWidget* page)
{
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* title = new QLabel(QStringLiteral("Roundtable is public"));
    title->setObjectName(QStringLiteral("roundtableFirstRunTitle"));
    layout->addWidget(title);

    auto* intro = new QLabel(QStringLiteral(
        "Roundtable shows who hears whom, live, to everyone on it and on its web page. "
        "With your station on, they see:"));
    intro->setWordWrap(true);
    layout->addWidget(intro);

    const QString call = SpotSourceHost::roundtableCallsign().toUpper();
    const QString grid = RoundtableWire::gridToSend(SpotSourceHost::roundtableGridSquare());
    QString who;
    if (call.isEmpty()) {
        who = QStringLiteral("Your callsign and grid");
    } else if (grid.isEmpty()) {
        who = QStringLiteral("Your callsign %1").arg(youMark(call));
    } else {
        who = QStringLiteral("Your callsign %1 and grid %2").arg(youMark(call), youMark(grid));
    }
    auto* list = new QVBoxLayout;
    list->setSpacing(0);
    list->addWidget(item(glyph("#00d4ff", "#00d4ff"), who));
    list->addWidget(item(glyph("#4caf6a", "#4caf6a"),
                         QStringLiteral("Your radio's reading of each talker, and how far you "
                                        "are from them")));
    list->addWidget(item(glyph("#33280f", "#d8a55f"),
                         QStringLiteral("Every station's reading of you while you talk")));
    list->addWidget(item(glyph("transparent", "#909096"),
                         QStringLiteral("Messages you post on your frequency")));
    layout->addLayout(list);

    auto* archive = new QLabel(QStringLiteral("Your archive of who heard you stays private to you."));
    archive->setObjectName(QStringLiteral("roundtableFirstRunArchive"));
    archive->setWordWrap(true);
    layout->addWidget(archive);

    auto* hideRow = new QFrame;
    hideRow->setObjectName(QStringLiteral("roundtableFirstRunHideRow"));
    auto* hideLayout = new QHBoxLayout(hideRow);
    hideLayout->setContentsMargins(12, 10, 12, 10);
    hideLayout->setSpacing(12);
    auto* hideText = new QVBoxLayout;
    hideText->setSpacing(2);
    auto* hideTitle = new QLabel(QStringLiteral("Hide my station"));
    hideTitle->setObjectName(QStringLiteral("roundtableFirstRunHideTitle"));
    hideText->addWidget(hideTitle);
    auto* hideHelp = new QLabel(QStringLiteral(
        "You still see everything. Your station and your readings are not sent. "
        "Others can still hear you, and may name you when you talk."));
    hideHelp->setObjectName(QStringLiteral("roundtableFirstRunHideText"));
    hideHelp->setWordWrap(true);
    hideText->addWidget(hideHelp);
    hideLayout->addLayout(hideText, 1);
    m_hideSwitch = new QPushButton(QStringLiteral("Off"));
    m_hideSwitch->setObjectName(QStringLiteral("roundtableFirstRunHideSwitch"));
    m_hideSwitch->setCheckable(true);
    m_hideSwitch->setAccessibleName(QStringLiteral("Hide my station"));
    connect(m_hideSwitch, &QPushButton::toggled, this, [this](bool) { applySwitch(); });
    hideLayout->addWidget(m_hideSwitch, 0, Qt::AlignVCenter);
    layout->addWidget(hideRow);

    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(10);
    m_stayHidden = new QPushButton(QStringLiteral("Stay hidden"));
    m_stayHidden->setObjectName(QStringLiteral("roundtableFirstRunStayHidden"));
    m_goLive = new QPushButton(QStringLiteral("Go live"));
    m_goLive->setObjectName(QStringLiteral("roundtableFirstRunGoLive"));
    connect(m_stayHidden, &QPushButton::clicked, this, [this] { answer(false); });
    connect(m_goLive, &QPushButton::clicked, this, [this] { answer(true); });
    buttons->addWidget(m_stayHidden, 1);
    buttons->addWidget(m_goLive, 1);
    layout->addSpacing(4);
    layout->addLayout(buttons);

    m_error = new QLabel;
    m_error->setObjectName(QStringLiteral("roundtableFirstRunError"));
    m_error->setWordWrap(true);
    m_error->hide();
    layout->addWidget(m_error);

    auto* foot = new QLabel(QStringLiteral(
        "Asked once for this station, on any of its devices. Change it any time in "
        "Roundtable settings."));
    foot->setObjectName(QStringLiteral("roundtableFirstRunFoot"));
    foot->setAlignment(Qt::AlignHCenter);
    foot->setWordWrap(true);
    layout->addWidget(foot);

    applySwitch();
}

void RoundtableFirstRunDialog::buildNote(QWidget* page)
{
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto* title = new QLabel(QStringLiteral("Roundtable is public"));
    title->setObjectName(QStringLiteral("roundtableFirstRunTitle"));
    layout->addWidget(title);

    m_noteHead = new QLabel;
    m_noteHead->setObjectName(QStringLiteral("roundtableFirstRunNoteHead"));
    m_noteHead->setWordWrap(true);
    layout->addWidget(m_noteHead);

    auto* body = new QLabel(QStringLiteral(
        "Asked once per station, so this computer doesn't ask again. Hide my station is in "
        "Roundtable settings."));
    body->setObjectName(QStringLiteral("roundtableFirstRunNoteText"));
    body->setWordWrap(true);
    layout->addWidget(body);

    auto* row = new QHBoxLayout;
    row->addStretch(1);
    auto* ok = new QPushButton(QStringLiteral("OK"));
    ok->setObjectName(QStringLiteral("roundtableFirstRunNoteOk"));
    ok->setStyleSheet(QLatin1String(kMainButtonStyle));
    ok->setMinimumWidth(140);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    row->addWidget(ok);
    layout->addLayout(row);
    layout->addStretch(1);
}

bool RoundtableFirstRunDialog::hideSwitchOn() const
{
    return m_hideSwitch != nullptr && m_hideSwitch->isChecked();
}

void RoundtableFirstRunDialog::setHideSwitch(bool on)
{
    if (m_hideSwitch != nullptr) {
        m_hideSwitch->setChecked(on);
    }
}

void RoundtableFirstRunDialog::applySwitch()
{
    const bool on = hideSwitchOn();
    m_hideSwitch->setText(on ? QStringLiteral("On") : QStringLiteral("Off"));
    // The switch moves the main button; either button still answers.
    m_stayHidden->setStyleSheet(QLatin1String(on ? kMainButtonStyle : kQuietButtonStyle));
    m_goLive->setStyleSheet(QLatin1String(on ? kQuietButtonStyle : kMainButtonStyle));
    m_stayHidden->setDefault(on);
    m_goLive->setDefault(!on);
}

void RoundtableFirstRunDialog::answer(bool live)
{
    if (m_mode != Mode::Question || m_sent) {
        return;
    }
    if (!m_model) {
        m_error->setText(QStringLiteral("Roundtable is not running here."));
        m_error->show();
        return;
    }
    m_sent = true;
    m_error->hide();
    m_goLive->setEnabled(false);
    m_stayHidden->setEnabled(false);
    // The Core's own client answers at once, a window's Core when its
    // `roundtable` object says so: either way onFirstRunChanged closes the
    // sheet, and a refusal comes back through onRefused.
    m_model->answerFirstRun(live);
}

void RoundtableFirstRunDialog::onFirstRunChanged()
{
    if (!m_model || !m_model->firstRunAnswered() || m_mode != Mode::Question) {
        return;
    }
    if (m_sent) {
        markSeen();
        accept();
        return;
    }
    // Another device answered while this one was asking.
    showNote();
}

void RoundtableFirstRunDialog::onRefused(const QString& verb, const QString& reason)
{
    if (verb != QLatin1String("answerFirstRun") || !m_sent || m_mode != Mode::Question) {
        return;
    }
    m_sent = false;
    m_goLive->setEnabled(true);
    m_stayHidden->setEnabled(true);
    if (m_model && m_model->firstRunAnswered()) {
        showNote();
        return;
    }
    m_error->setText(reason);
    m_error->show();
}

void RoundtableFirstRunDialog::showNote()
{
    m_mode = Mode::Note;
    const bool hidden = m_model && m_model->hidden();
    m_noteHead->setText(hidden
        ? QStringLiteral("<b>Your station chose Stay hidden on another device.</b>")
        : QStringLiteral("<b>Your station went live from another device.</b>"));
    m_question->hide();
    m_note->show();
    adjustSize();
    markSeen();
}

void RoundtableFirstRunDialog::markSeen()
{
    auto& settings = AppSettings::instance();
    if (settings.value(QLatin1String(kSeenKey)).toString() != QLatin1String("True")) {
        settings.setValue(QLatin1String(kSeenKey), QStringLiteral("True"));
        settings.save();
    }
}

} // namespace NereusSDR
