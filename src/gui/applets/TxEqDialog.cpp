// =================================================================
// src/gui/applets/TxEqDialog.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/eqform.cs (legacy 10-band TX EQ —
//   grpTXEQ, lines 1021-1561 [v2.10.3.13]; parametric panel —
//   lines 235-2911 [v2.10.3.13]).  Original licence from Thetis
//   source is included below.
//
// Layout faithfully mirrors Thetis grpTXEQ for the legacy panel:
//   - 1 Enable checkbox  (chkTXEQEnabled — eqform.cs:74)
//   - 1 preamp slider    (tbTXEQPre, range -12..+15 dB, eqform.cs:1549-1561)
//   - 10 band gain sliders (tbTXEQ0..9, range -12..+15 dB, eqform.cs:1496-1546)
//   - 10 band freq spinboxes (udTXEQ0..9, range 0..20000 Hz, eqform.cs:1329)
//
// Thetis pairs the spinbox (numeric) with the slider (visual) per
// band — the spinbox is the FREQUENCY of that band's center, NOT a
// gain mirror.  We keep that semantics verbatim and ALSO add a
// lightweight gain-spinbox mirror under each gain slider so users
// can type exact dB values (NereusSDR-spin, common Qt6 idiom).
//
// Parametric panel (3M-3a-ii follow-up Batch 9):
//   - chkLegacyEQ checkbox at top toggles between legacy / parametric
//     (eqform.cs:969-981 + 2862-2911 [v2.10.3.13])
//   - Single ParametricEqWidget (replaces Thetis ucParametricEq1)
//     with edit row above + right column of controls
//   - Edit row: # / f / Gain / Q / Preamp + Reset (mirrors
//     nudParaEQ_selected_band / nudParaEQ_f / nudParaEQ_gain /
//     nudParaEQ_q / nudParaEQ_preamp / btnParaEQReset at
//     eqform.cs:235-273 + 731-926 [v2.10.3.13])
//   - Right column: Log scale / Use Q Factors / Live Update + warning
//     icon / Low / High freq spinboxes / 5/10/18 band radios
//     (mirrors chkLogScale / chkUseQFactors / chkPanaEQ_live /
//     pbParaEQ_live_warning / udParaEQ_low / udParaEQ_high /
//     radParaEQ_5/10/18 at eqform.cs:241-275 + 402-600 [v2.10.3.13])
//
// Out-of-scope permanently (handled elsewhere):
//   - RX EQ controls — Thetis EQForm hosts both; NereusSDR splits them.
//     The RX EQ widget lives in EqApplet.
//   - Profile management — Thetis hosts the same profile bank on EQForm;
//     NereusSDR exposes it on the TxApplet's TX-Profile combo and the
//     Console / Setup → Audio → TX Profile editor.  Re-introducing a
//     redundant copy on this dialog would only confuse users.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-02 — Native dual-mode editor and exact session history by
//                 J.J. Boyd (KG4VCF), assisted by OpenAI Codex.
//   2026-04-29 — Phase 3M-3a-i Batch 3 (Task A.1): created by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//   2026-04-29 — Phase 3M-3a-i Batch 4 (Task A.2): TX profile combo
//                 + Save / Save As / Delete buttons added, wired to
//                 RadioModel::micProfileManager().  Mirrored the Thetis
//                 setup.cs:9505-9656 [v2.10.3.13] handler set
//                 (comboTXProfileName_SelectedIndexChanged,
//                  btnTXProfileSave_Click, btnTXProfileDelete_Click).
//   2026-04-30 — Phase 3M-3a-ii follow-up Batch 9: legacy /
//                 parametric panel toggle added by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude
//                 Code.  Profile combo + Save / Save As / Delete
//                 dropped (TxApplet hosts them).  chkLegacyEQ (default
//                 checked) swaps a QStackedWidget between the original
//                 legacy slider panel and a new parametric panel
//                 (single ParametricEqWidget + edit row + right column).
//                 The chkLegacyEQ state persists in AppSettings under
//                 "TxEqDialog/UsingLegacyEQ" to preserve user choice
//                 across launches (mirrors Thetis's
//                 _state.UsingLegacyEQ Common.RestoreForm round-trip).
//                 Legacy band-column sliders / spinboxes / headers now
//                 pick up Style::sliderVStyle() + kSpinBoxStyle +
//                 kTextPrimary — fixes a styling regression where they
//                 rendered with the system default look-and-feel.
//   2026-04-30 — Phase 3M-3a-ii follow-up sub-PR style fix: added a
//                 dialog-level QSS block in the constructor so the
//                 parametric panel's default Qt6 widgets (spinboxes,
//                 combos, radios, checkboxes, group-boxes, labels,
//                 push-buttons) and the top-strip Enable/Nc/Mp/Cutoff/
//                 Window controls pick up the project's dark theme.
//                 The Batch 9 per-widget styles in the legacy band
//                 columns retain their per-widget specificity.  J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

//=================================================================
// eqform.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "TxEqDialog.h"

#include "core/AppSettings.h"
#include "core/ParaEqEnvelope.h"
#include "core/MicProfileManager.h"
#include "core/TxChannel.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/ParametricEqWidget.h"
#include "gui/widgets/EqEditHistory.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include <QApplication>
#include <QAbstractSpinBox>
#include <QDataStream>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QScreen>
#include <QScrollArea>
#include <QScopedValueRollback>
#include <QTimer>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

#include <cmath>

namespace NereusSDR {

namespace {

// Sentinel property-name for the band index attached to each
// slider/spinbox so the shared sender-finder slot can resolve which
// band fired.
constexpr const char* kBandIndexProp = "txEqBandIndex";

// AppSettings key for the legacy-vs-parametric toggle state.
// Mirrors Thetis _state.UsingLegacyEQ (eqform.cs:2866) round-tripped
// through Common.RestoreForm.
constexpr const char* kLegacyToggleSettingsKey = "TxEqDialog/UsingLegacyEQ";

// Parametric panel widget defaults — sourced byte-for-byte from
// eqform.cs:928-967 [v2.10.3.13] (ucParametricEq1 widget property
// block in the InitializeComponent generated code).
constexpr double kParaDefaultDbMin       = -24.0;          // cs:944
constexpr double kParaDefaultDbMax       =  24.0;          // cs:943
constexpr double kParaDefaultMinHz       =   0.0;          // cs:947
constexpr double kParaDefaultMaxHz       = 2700.0;         // cs:946
constexpr double kParaDefaultQMin        =   0.2;          // cs:954
constexpr double kParaDefaultQMax        =  20.0;          // cs:953
constexpr double kParaDefaultGlobalGainDb=   0.0;          // cs:948
constexpr double kParaDefaultMinPointSpacingHz = 5.0;      // cs:950
constexpr int    kParaDefaultBandShadeAlpha = 70;          // cs:938
constexpr int    kParaDefaultAxisTickLength = 6;           // cs:936

// Right-column controls — sourced byte-for-byte from
// eqform.cs:540-600 [v2.10.3.13] (udParaEQ_low / udParaEQ_high) and
// cs:493-526 (radParaEQ_5/10/18 default 10).
constexpr int    kParaLowMinHz           =      0;         // cs:559
constexpr int    kParaLowMaxHz           =  20000;         // cs:551
constexpr int    kParaLowDefaultHz       =      0;         // cs:565
constexpr int    kParaHighMinHz          =      0;         // cs:589
constexpr int    kParaHighMaxHz          =  20000;         // cs:581
constexpr int    kParaHighDefaultHz      =  16000;         // cs:595

// 1 kHz minimum spread between Low and High — mirrors
// frmCFCConfig.cs:122-138 [v2.10.3.13] guard (eqform doesn't expose
// the same constant explicitly but enforces the same invariant via
// nudParaEQ_low / nudParaEQ_high handlers; we copy CFC's threshold).
constexpr int    kMinFreqSpreadHz        = 1000;

// Edit-row preamp (nudParaEQ_preamp) — eqform.cs:671-699 [v2.10.3.13].
constexpr double kParaPreampMinDb        = -24.0;          // cs:685-689
constexpr double kParaPreampMaxDb        =  24.0;          // cs:680-684


} // namespace

// ─────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────

TxEqDialog::TxEqDialog(RadioModel* radio, QWidget* parent)
    : QDialog(parent)
    , m_radio(radio)
{
    setWindowTitle(tr("TX Equalizer"));
    setObjectName(QStringLiteral("TxEqDialog"));
    // Modeless: don't block other interaction.
    setModal(false);
    // Singleton lifecycle — caller owns the pointer via instance().
    // Default WA_DeleteOnClose is false, but make it explicit so the
    // singleton survives close/hide cycles.
    setAttribute(Qt::WA_DeleteOnClose, false);

    // Dialog-level QSS — without this, default Qt6 widgets (spinboxes,
    // combos, radios, checkboxes, group-boxes, labels) render with the
    // system default theme inside an otherwise-dark dialog, producing the
    // dark-on-dark "invisible boxes" reported during the 3M-3a-ii
    // follow-up bench test.  The block layers on the project's
    // StyleConstants helpers via QSS selectors; per-widget setStyleSheet
    // calls in buildBandColumn() / buildLegacyPanel() (Batch 9 fix)
    // keep their own specificity and continue to override.
    setStyleSheet(QString::fromLatin1(NereusSDR::Style::kPageStyle)
                  + QString::fromLatin1(NereusSDR::Style::kGroupBoxStyle)
                  + QString::fromLatin1(NereusSDR::Style::kSpinBoxStyle)
                  + NereusSDR::Style::doubleSpinBoxStyle()
                  + QString::fromLatin1(NereusSDR::Style::kComboStyle)
                  + QString::fromLatin1(NereusSDR::Style::kCheckBoxStyle)
                  + QString::fromLatin1(NereusSDR::Style::kRadioButtonStyle)
                  + QString::fromLatin1(NereusSDR::Style::kButtonStyle)
                  + QStringLiteral("QPushButton:checked { border: 1px solid %1; color: %1; } ").arg(Style::kAccent));

    m_history[0] = new EqEditHistory(this);
    m_history[1] = new EqEditHistory(this);
    buildUi();
    m_seedGraph = m_parametricWidget->saveEditState();
    wireSignals();
    syncFromModel();
    updateEditRowFromSelection();
    rebaseEditHistory();
}

TxEqDialog::~TxEqDialog()
{
    // Finish focused numeric edits while selection/history members are alive;
    // QDialog's base destructor otherwise hides after those members are gone.
    hide();
}

TxEqDialog* TxEqDialog::instance(RadioModel* radio, QWidget* parent)
{
    static QPointer<TxEqDialog> s_instance;
    if (s_instance.isNull()) {
        s_instance = new TxEqDialog(radio, parent);
    }
    return s_instance.data();
}

// ─────────────────────────────────────────────────────────────────────
// UI build-out
// ─────────────────────────────────────────────────────────────────────

void TxEqDialog::buildUi()
{
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(8, 8, 8, 8);
    outer->setSpacing(6);

    QHBoxLayout* modeRow = new QHBoxLayout;
    m_modeSelector = new QButtonGroup(this);
    m_modeSelector->setObjectName(QStringLiteral("TxEqModeSelector"));
    const bool legacy = AppSettings::instance().value(QLatin1String(kLegacyToggleSettingsKey), QStringLiteral("True")).toString() == QStringLiteral("True");
    for (int i = 0; i < 2; ++i) {
        auto* button = new QPushButton(i == 0 ? tr("Graphic · Legacy") : tr("Parametric"), this);
        button->setCheckable(true); button->setAutoDefault(false);
        button->setAccessibleName(i == 0 ? tr("Graphic EQ mode") : tr("Parametric EQ mode"));
        m_modeSelector->addButton(button, i); button->setChecked(i == (legacy ? 0 : 1));
        modeRow->addWidget(button);
    }
    modeRow->addStretch();
    m_undoBtn = new QPushButton(tr("Undo"), this); m_undoBtn->setObjectName(QStringLiteral("TxEqUndoBtn"));
    m_redoBtn = new QPushButton(tr("Redo"), this); m_redoBtn->setObjectName(QStringLiteral("TxEqRedoBtn"));
    m_undoBtn->setAutoDefault(false); m_redoBtn->setAutoDefault(false);
    m_undoBtn->setToolTip(tr("Undo EQ edit (Ctrl+Z / Command+Z)"));
    m_redoBtn->setToolTip(tr("Redo EQ edit (Ctrl+Shift+Z / Command+Shift+Z)"));
    modeRow->addWidget(m_undoBtn); modeRow->addWidget(m_redoBtn);
    outer->addLayout(modeRow);
    m_enableChk = new QCheckBox(tr("Enable TX EQ"), this);
    m_enableChk->setObjectName(QStringLiteral("TxEqEnableChk"));
    outer->addWidget(m_enableChk);
    auto* advancedToggle = new QPushButton(tr("Advanced"), this);
    advancedToggle->setObjectName(QStringLiteral("TxEqAdvancedToggle"));
    advancedToggle->setCheckable(true); advancedToggle->setAutoDefault(false);
    auto* advanced = new QWidget(this); advanced->setObjectName(QStringLiteral("TxEqAdvancedControls"));
    advanced->hide();
    auto* advancedLayout = new QVBoxLayout(advanced);
    connect(advancedToggle, &QPushButton::toggled, advanced, &QWidget::setVisible);

    // ── Top strip: Enable + WDSP filter combos ──────────────────────
    QHBoxLayout* topRow = new QHBoxLayout;
    topRow->setSpacing(10);

    {
        QLabel* lbl = new QLabel(tr("Nc:"), this);
        topRow->addWidget(lbl);
        m_ncSpin = new QSpinBox(this);
        m_ncSpin->setObjectName(QStringLiteral("TxEqNcSpin"));
        m_ncSpin->setRange(32, 8192);
        m_ncSpin->setSingleStep(32);
        m_ncSpin->setToolTip(
            tr("Number of filter coefficients used by the WDSP EQ stage.  "
               "Higher values give sharper filter shapes but use more CPU.  "
               "Defaults to 2048 — change only if you know what you're doing."));
        topRow->addWidget(m_ncSpin);
    }

    topRow->addSpacing(10);

    m_mpChk = new QCheckBox(tr("Mp"), this);
    m_mpChk->setObjectName(QStringLiteral("TxEqMpChk"));
    m_mpChk->setToolTip(
        tr("Minimum-phase mode.  Off = linear-phase (no group delay "
           "distortion).  On = minimum-phase (lower latency, slight "
           "phase non-linearity).  Default off."));
    topRow->addWidget(m_mpChk);

    topRow->addSpacing(10);

    {
        QLabel* lbl = new QLabel(tr("Cutoff:"), this);
        topRow->addWidget(lbl);
        m_ctfmodeCombo = new QComboBox(this);
        m_ctfmodeCombo->setObjectName(QStringLiteral("TxEqCtfmodeCombo"));
        // Items match WDSP eq.c create_eqp() ctfmode parameter.
        m_ctfmodeCombo->addItem(tr("0 — Peaking"));
        m_ctfmodeCombo->addItem(tr("1 — Notch"));
        m_ctfmodeCombo->setToolTip(
            tr("Filter cutoff mode for band shapes.  Peaking = "
               "bell-shaped boost/cut.  Notch = sharp band-stop.  "
               "Default Peaking."));
        topRow->addWidget(m_ctfmodeCombo);
    }

    topRow->addSpacing(10);

    {
        QLabel* lbl = new QLabel(tr("Window:"), this);
        topRow->addWidget(lbl);
        m_wintypeCombo = new QComboBox(this);
        m_wintypeCombo->setObjectName(QStringLiteral("TxEqWintypeCombo"));
        // Items match WDSP eq.c create_eqp() wintype parameter.
        m_wintypeCombo->addItem(tr("0 — Blackman-Harris"));
        m_wintypeCombo->addItem(tr("1 — Hann"));
        m_wintypeCombo->setToolTip(
            tr("Window function used when constructing the EQ filter "
               "impulse response.  Blackman-Harris = sharper rejection.  "
               "Hann = smoother rolloff.  Default Blackman-Harris."));
        topRow->addWidget(m_wintypeCombo);
    }

    topRow->addStretch(1);
    advancedLayout->addLayout(topRow);

    QFrame* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    outer->addWidget(sep);

    // ── Stacked legacy + parametric panels.  The chkLegacyEQ checkbox
    // toggles which one is visible.
    m_panelStack = new QStackedWidget(this);
    m_panelStack->setObjectName(QStringLiteral("TxEqPanelStack"));
    m_legacyPanel = buildLegacyPanel();
    m_parametricPanel = buildParametricPanel();
    m_panelStack->addWidget(m_legacyPanel);       // index 0
    m_panelStack->addWidget(m_parametricPanel);   // index 1
    m_panelStack->setCurrentIndex(legacy ? 0 : 1);
    outer->addWidget(m_panelStack, 1);
    // Algorithm controls and the parametric range/log/live controls share
    // one collapsible section, reachable in either mode.
    auto* paraAdvanced = m_parametricPanel->findChild<QWidget*>(QStringLiteral("TxEqParaAdvancedControls"));
    advancedLayout->addWidget(paraAdvanced);
    outer->addWidget(advancedToggle);
    auto* advancedScroll = new QScrollArea(this); advancedScroll->setWidgetResizable(true);
    advancedScroll->setFrameShape(QFrame::NoFrame); advancedScroll->setWidget(advanced);
    advancedScroll->setMaximumHeight(180); advancedScroll->hide();
    connect(advancedToggle, &QPushButton::toggled, advancedScroll, &QWidget::setVisible);
    outer->addWidget(advancedScroll);
    paraAdvanced->setVisible(!legacy);
    for (auto* button : findChildren<QPushButton*>()) { button->setAutoDefault(false); }
    setMinimumSize(500, 400);
    const QSize available = screen() ? screen()->availableGeometry().size() : QSize(1280, 800);
    resize(qMin(1000, available.width() - 40), qMin(720, available.height() - 60));
}

// ─────────────────────────────────────────────────────────────────────
// Legacy panel build — From Thetis grpTXEQ at eqform.cs:1021-1561 [v2.10.3.13].
// Preserved verbatim from the Phase 3M-3a-i Batch 3 implementation; the
// only change in Batch 9 is the per-column slider/spinbox style fix
// applied below.
// ─────────────────────────────────────────────────────────────────────

namespace {

// Build a vertical column for one band (or the preamp).  Returns the
// outer column widget; populates *outSlider / *outDbSpin / *outFreqSpin
// (any may be null — the preamp column has no freq spin).
//
// Batch 9 styling fix — applies Style::sliderVStyle() to the slider,
// Style::kSpinBoxStyle to both spinboxes, and a kTextPrimary colour
// override to the header label.  Without this, the band columns
// rendered with the system default theme inside an otherwise-dark
// dialog (the header was hard-to-read system grey, the slider used
// the system handle, and the spinboxes lacked the project bevel).
QWidget* buildBandColumn(const QString& headerLabel,
                         int bandIndex,            // -1 for preamp
                         QSlider** outSlider,
                         QSpinBox** outDbSpin,
                         QSpinBox** outFreqSpin,
                         QWidget* parent)
{
    QWidget* col = new QWidget(parent);
    QVBoxLayout* v = new QVBoxLayout(col);
    v->setContentsMargins(2, 2, 2, 2);
    v->setSpacing(3);
    v->setAlignment(Qt::AlignTop);

    QLabel* hdr = new QLabel(headerLabel, col);
    hdr->setAlignment(Qt::AlignHCenter);
    QFont f = hdr->font();
    f.setBold(true);
    hdr->setFont(f);
    // Batch 9 — apply project text colour so the band header reads
    // against the dark dialog background.
    hdr->setStyleSheet(QStringLiteral("color: %1;")
                          .arg(NereusSDR::Style::kTextPrimary));
    v->addWidget(hdr);

    QSlider* s = new QSlider(Qt::Vertical, col);
    s->setRange(TransmitModel::kTxEqBandDbMin, TransmitModel::kTxEqBandDbMax);
    s->setTickPosition(QSlider::TicksBothSides);
    s->setTickInterval(3);          // matches Thetis tbTXEQ0.TickFrequency = 3
    s->setPageStep(3);              // matches LargeChange = 3
    s->setFixedHeight(180);
    s->setProperty(kBandIndexProp, bandIndex);
    // Batch 9 — apply the project vertical slider style.
    s->setStyleSheet(NereusSDR::Style::sliderVStyle() + QStringLiteral("QSlider::handle:vertical { height: 4px; border-radius: 1px; margin: 0 -6px; }"));
    v->addWidget(s, 1, Qt::AlignHCenter);
    *outSlider = s;

    QSpinBox* db = new QSpinBox(col);
    db->setRange(TransmitModel::kTxEqBandDbMin, TransmitModel::kTxEqBandDbMax);
    db->setSuffix(QString());
    db->setFixedWidth(62);
    db->setProperty(kBandIndexProp, bandIndex);
    // Batch 9 — apply the project spinbox style.
    db->setStyleSheet(NereusSDR::Style::kSpinBoxStyle);
    v->addWidget(db);
    auto* dbUnit = new QLabel(QObject::tr("dB"), col); dbUnit->setAlignment(Qt::AlignHCenter); v->addWidget(dbUnit);
    *outDbSpin = db;

    if (outFreqSpin) {
        QSpinBox* hz = new QSpinBox(col);
        hz->setRange(TransmitModel::kTxEqFreqHzMin,
                     TransmitModel::kTxEqFreqHzMax);
        hz->setSuffix(QString());
        hz->setProperty(kBandIndexProp, bandIndex);
        // Allow 4-digit + suffix room.
        hz->setFixedWidth(62);
        hz->setButtonSymbols(QAbstractSpinBox::NoButtons);
        // Batch 9 — apply the project spinbox style.
        hz->setStyleSheet(NereusSDR::Style::kSpinBoxStyle);
        v->addWidget(hz);
        auto* hzUnit = new QLabel(QObject::tr("Hz"), col); hzUnit->setAlignment(Qt::AlignHCenter); v->addWidget(hzUnit);
        *outFreqSpin = hz;
    }

    if (!outFreqSpin) { v->addSpacing(db->sizeHint().height() + dbUnit->sizeHint().height() + 6); }
    v->addStretch(1);
    return col;
}

} // anonymous namespace

QWidget* TxEqDialog::buildLegacyPanel()
{
    QWidget* root = new QWidget(this);
    root->setObjectName(QStringLiteral("TxEqLegacyPanel"));
    auto* rootLayout = new QVBoxLayout(root); rootLayout->setContentsMargins(0, 0, 0, 0);
    auto* panelScroll = new QScrollArea(root); panelScroll->setWidgetResizable(true);
    panelScroll->setFrameShape(QFrame::NoFrame); panelScroll->setObjectName(QStringLiteral("TxEqLegacyScroll"));
    QWidget* panel = new QWidget(panelScroll);
    panelScroll->setWidget(panel); rootLayout->addWidget(panelScroll);
    QVBoxLayout* legacy = new QVBoxLayout(panel);
    legacy->setContentsMargins(0, 0, 0, 0);
    legacy->setSpacing(6);

    // ── Band columns row (preamp + 10 bands + dB scale label) ───────
    QGroupBox* bandGroup = new QGroupBox(tr("TX EQ Bands"), panel);
    bandGroup->setMaximumHeight(380);
    QHBoxLayout* bandRow = new QHBoxLayout(bandGroup);
    bandRow->setContentsMargins(8, 14, 8, 8);
    bandRow->setSpacing(2);

    // Preamp column (no freq spinbox).
    {
        QSpinBox* dummyFreq = nullptr;  // unused
        Q_UNUSED(dummyFreq);
        QWidget* col = buildBandColumn(
            tr("Pre"), /*bandIndex=*/-1,
            &m_preampSlider, &m_preampSpin, /*outFreqSpin=*/nullptr,
            bandGroup);
        m_preampSlider->setRange(TransmitModel::kTxEqPreampDbMin,
                                 TransmitModel::kTxEqPreampDbMax);
        m_preampSpin->setRange(TransmitModel::kTxEqPreampDbMin,
                               TransmitModel::kTxEqPreampDbMax);
        m_preampSlider->setObjectName(QStringLiteral("TxEqPreampSlider"));
        m_preampSpin->setObjectName(QStringLiteral("TxEqPreampSpin"));
        m_preampSlider->setToolTip(
            tr("Overall TX EQ pre-gain applied before the per-band "
               "boosts/cuts.  Range -12 to +15 dB."));
        m_preampSpin->setToolTip(m_preampSlider->toolTip());
        bandRow->addWidget(col);
    }

    // 10 band columns.
    for (int i = 0; i < 10; ++i) {
        QWidget* col = buildBandColumn(
            tr("B%1").arg(i + 1), i,
            &m_bandSliders[i], &m_bandSpins[i], &m_freqSpins[i],
            bandGroup);
        m_bandSliders[i]->setObjectName(QStringLiteral("TxEqBandSlider%1").arg(i));
        m_bandSpins[i]->setObjectName(QStringLiteral("TxEqBandSpin%1").arg(i));
        m_freqSpins[i]->setObjectName(QStringLiteral("TxEqFreqSpin%1").arg(i));

        const QString gainTip = tr(
            "Band %1 gain.  Range -12 to +15 dB.").arg(i + 1);
        m_bandSliders[i]->setToolTip(gainTip);
        m_bandSpins[i]->setToolTip(gainTip);
        m_freqSpins[i]->setToolTip(
            tr("Band %1 center frequency in Hz.  Range %2 to %3 Hz.")
                .arg(i + 1)
                .arg(TransmitModel::kTxEqFreqHzMin)
                .arg(TransmitModel::kTxEqFreqHzMax));
        bandRow->addWidget(col);
    }

    // dB scale labels at the right edge — matches Thetis lblTXEQ15db /
    // lblTXEQ0dB / lblTXEQminus12db markers (eqform.cs:1358-1397).
    {
        QWidget* scaleCol = new QWidget(bandGroup);
        QVBoxLayout* v = new QVBoxLayout(scaleCol);
        v->setContentsMargins(4, 18, 0, 4);
        v->setSpacing(0);
        QLabel* top = new QLabel(QStringLiteral("+15 dB"), scaleCol);
        QLabel* mid = new QLabel(QStringLiteral("  0 dB"), scaleCol);
        QLabel* bot = new QLabel(QStringLiteral("-12 dB"), scaleCol);
        top->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        mid->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        bot->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
        // Batch 9 — match the band-column header styling so all dB
        // labels read against the dark dialog background.
        const QString scaleStyle = QStringLiteral("color: %1;")
                                       .arg(NereusSDR::Style::kTextPrimary);
        top->setStyleSheet(scaleStyle);
        mid->setStyleSheet(scaleStyle);
        bot->setStyleSheet(scaleStyle);
        v->addWidget(top);
        v->addStretch(1);
        v->addWidget(mid);
        v->addStretch(1);
        v->addWidget(bot);
        bandRow->addWidget(scaleCol);
    }

    legacy->addWidget(bandGroup, 1);
    auto* reset = new QPushButton(tr("Reset curve"), panel);
    reset->setObjectName(QStringLiteral("TxEqLegacyResetBtn")); reset->setAutoDefault(false);
    connect(reset, &QPushButton::clicked, this, [this] {
        beginEdit();
        { QSignalBlocker a(m_preampSlider), b(m_preampSpin); m_preampSlider->setValue(0); m_preampSpin->setValue(0); }
        for (int i = 0; i < 10; ++i) {
            QSignalBlocker a(m_bandSliders[i]), b(m_bandSpins[i]);
            m_bandSliders[i]->setValue(0); m_bandSpins[i]->setValue(0);
        }
        finishEdit();
    });
    legacy->addWidget(reset, 0, Qt::AlignLeft);
    legacy->addStretch(1);
    return root;
}

// ─────────────────────────────────────────────────────────────────────
// Parametric panel build — From Thetis pnlParaEQ + pnlParaEQ2 +
// ucParametricEq1 at eqform.cs:235-967 [v2.10.3.13].
//
// Native editor presentation keeps the configured graph above the selected
// controls and preserves the existing Thetis values/ranges. Advanced controls
// collapse; the independent band-button strip and numeric controls can scroll.
// ─────────────────────────────────────────────────────────────────────

QWidget* TxEqDialog::buildParametricPanel()
{
    QWidget* root = new QWidget(this);
    root->setObjectName(QStringLiteral("TxEqParametricPanel"));
    QWidget* panel = root;
    QVBoxLayout* col = new QVBoxLayout(panel);
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(6);

    QHBoxLayout* editRow = nullptr;
    // ── Edit row — eqform.cs:235-273 + 651-740 [v2.10.3.13].
    // Designer order: # / f / dB (gain) / Q / Preamp / Reset.
    {
        QHBoxLayout* row = new QHBoxLayout;
        row->setSpacing(6);

        // f — nudParaEQ_f (cs:267, max 20000 Hz).
        row->addSpacing(6);
        row->addWidget(new QLabel(tr("Frequency"), panel));
        m_paraFreqSpin = new QSpinBox(panel);
        m_paraFreqSpin->setObjectName(QStringLiteral("TxEqParaFreqSpin"));
        m_paraFreqSpin->setRange(0, 20000);
        m_paraFreqSpin->setSuffix(QStringLiteral(" Hz"));
        m_paraFreqSpin->setMinimumWidth(80);
        m_paraFreqSpin->setToolTip(tr(
            "Center frequency of the selected band in Hz."));
        row->addWidget(m_paraFreqSpin);

        // Gain — nudParaEQ_gain (cs:271, ±24 dB).
        row->addSpacing(6);
        row->addWidget(new QLabel(tr("Gain"), panel));
        m_paraGainSpin = new QDoubleSpinBox(panel);
        m_paraGainSpin->setObjectName(QStringLiteral("TxEqParaGainSpin"));
        m_paraGainSpin->setRange(kParaDefaultDbMin, kParaDefaultDbMax);
        m_paraGainSpin->setSuffix(QStringLiteral(" dB"));
        m_paraGainSpin->setDecimals(1);
        m_paraGainSpin->setSingleStep(0.5);
        m_paraGainSpin->setToolTip(tr(
            "Gain of the selected band in dB.  Range -24 to +24 dB."));
        row->addWidget(m_paraGainSpin);

        // Q — nudParaEQ_q (cs:269, range 0.2..20).
        row->addSpacing(6);
        row->addWidget(new QLabel(tr("Width (Q)"), panel));
        m_paraQSpin = new QDoubleSpinBox(panel);
        m_paraQSpin->setObjectName(QStringLiteral("TxEqParaQSpin"));
        m_paraQSpin->setRange(kParaDefaultQMin, kParaDefaultQMax);
        m_paraQSpin->setDecimals(2);
        m_paraQSpin->setSingleStep(0.1);
        m_paraQSpin->setToolTip(tr(
            "Quality factor (bandwidth) of the selected band.  "
            "Range 0.2 (very wide) to 20 (very narrow)."));
        row->addWidget(m_paraQSpin);

        // Preamp — nudParaEQ_preamp (cs:671-699, ±24 dB).
        row->addSpacing(6);
        row->addWidget(new QLabel(tr("Preamp"), panel));
        m_paraPreampSpin = new QDoubleSpinBox(panel);
        m_paraPreampSpin->setObjectName(QStringLiteral("TxEqParaPreampSpin"));
        m_paraPreampSpin->setRange(kParaPreampMinDb, kParaPreampMaxDb);
        m_paraPreampSpin->setSuffix(QStringLiteral(" dB"));
        m_paraPreampSpin->setDecimals(1);
        m_paraPreampSpin->setSingleStep(0.5);
        m_paraPreampSpin->setToolTip(tr(
            "Global preamp applied across all bands.  Range -24 to +24 dB."));
        row->addWidget(m_paraPreampSpin);

        row->addStretch(1);

        // Reset — btnParaEQReset (cs:731-740).
        m_paraResetBtn = new QPushButton(tr("Reset"), panel);
        m_paraResetBtn->setObjectName(QStringLiteral("TxEqParaResetBtn"));
        m_paraResetBtn->setToolTip(tr(
            "Reset all parametric bands to a flat curve."));
        row->addWidget(m_paraResetBtn);

        editRow = row;
    }

    // ── Widget + right column ──────────────────────────────────────
    QHBoxLayout* mainRow = new QHBoxLayout;
    mainRow->setSpacing(8);

    // ParametricEqWidget centered (replaces ucParametricEq1).
    m_parametricWidget = new ParametricEqWidget(panel);
    m_parametricWidget->setObjectName(QStringLiteral("TxEqParametricWidget"));
    // Apply Thetis defaults — ucParametricEq1 widget property block at
    // eqform.cs:928-967 [v2.10.3.13].
    m_parametricWidget->setDbMin(kParaDefaultDbMin);
    m_parametricWidget->setDbMax(kParaDefaultDbMax);
    m_parametricWidget->setFrequencyMinHz(kParaDefaultMinHz);
    m_parametricWidget->setFrequencyMaxHz(kParaDefaultMaxHz);
    m_parametricWidget->setQMin(kParaDefaultQMin);
    m_parametricWidget->setQMax(kParaDefaultQMax);
    m_parametricWidget->setGlobalGainDb(kParaDefaultGlobalGainDb);
    m_parametricWidget->setMinPointSpacingHz(kParaDefaultMinPointSpacingHz);
    m_parametricWidget->setBandShadeAlpha(kParaDefaultBandShadeAlpha);
    m_parametricWidget->setAxisTickLength(kParaDefaultAxisTickLength);
    m_parametricWidget->setShowAxisScales(true);            // cs:956
    m_parametricWidget->setShowBandShading(true);           // cs:957
    m_parametricWidget->setShowDotReadings(false);           // cs:958
    m_parametricWidget->setShowReadout(false);              // cs:959
    m_parametricWidget->setUsePerBandColours(true);         // cs:962
    m_parametricWidget->setAllowPointReorder(true);         // cs:930
    m_parametricWidget->setParametricEq(true);              // cs:952
    m_parametricWidget->setBandCount(10);                   // default 10-band
    m_parametricWidget->setMinimumSize(400, 180);
    m_parametricWidget->setEditorPresentationEnabled(true);
    mainRow->addWidget(m_parametricWidget, 1);

    // Right column — eqform.cs:241-275 + 402-600 [v2.10.3.13]:
    //   Log scale + Use Q Factors + Live Update + warning icon
    //   + Low / High freq spinboxes
    //   + 5/10/18 band radios
    auto* paraAdvanced = new QWidget(panel);
    paraAdvanced->setObjectName(QStringLiteral("TxEqParaAdvancedControls"));
    QVBoxLayout* rightCol = new QVBoxLayout(paraAdvanced);
    rightCol->setSpacing(6);

    // chkLogScale — eqform.cs:468-478 (default unchecked).
    m_paraLogScaleChk = new QCheckBox(tr("Log scale"), panel);
    m_paraLogScaleChk->setObjectName(QStringLiteral("TxEqParaLogScaleChk"));
    m_paraLogScaleChk->setToolTip(tr(
        "Render the parametric curve on a log frequency axis."));
    rightCol->addWidget(m_paraLogScaleChk);

    // chkUseQFactors — eqform.cs:528-540 (default checked).
    m_paraUseQFactorsChk = new QCheckBox(tr("Use Q Factors"), panel);
    m_paraUseQFactorsChk->setObjectName(QStringLiteral("TxEqParaUseQFactorsChk"));
    m_paraUseQFactorsChk->setChecked(true);
    m_paraUseQFactorsChk->setToolTip(tr(
        "Use Q factors per band when computing the EQ profile.  When off, "
        "the Q columns are ignored and the curve degenerates to flat-band."));


    // chkPanaEQ_live + warning icon — eqform.cs:402-414 + 388-400.
    {
        QHBoxLayout* liveRow = new QHBoxLayout;
        liveRow->setSpacing(4);
        liveRow->setContentsMargins(0, 0, 0, 0);

        m_paraLiveUpdateChk = new QCheckBox(tr("Live Update"), panel);
        m_paraLiveUpdateChk->setObjectName(
            QStringLiteral("TxEqParaLiveUpdateChk"));
        m_paraLiveUpdateChk->setToolTip(tr(
            "Push EQ values to the radio while dragging points.  "
            "When off, the WDSP profile is updated only on mouse-release."));
        liveRow->addWidget(m_paraLiveUpdateChk);

        // Warning icon — eqform.cs:388-400 (pbParaEQ_live_warning.Visible
        // = false until live mode is engaged).  Use the system warning
        // icon as a stand-in for Thetis's bundled bitmap.
        QLabel* warnIcon = new QLabel(panel);
        warnIcon->setObjectName(QStringLiteral("TxEqParaLiveWarning"));
        const QIcon icon = style()->standardIcon(QStyle::SP_MessageBoxWarning);
        warnIcon->setPixmap(icon.pixmap(16, 16));
        warnIcon->setToolTip(tr(
            "Live updates may take a while due to large DSP buffer sizes.  "
            "UI interactions may stutter while the profile is rebuilt."));
        warnIcon->setVisible(false);  // matches Thetis cs:400 default
        liveRow->addWidget(warnIcon);
        liveRow->addStretch(1);
        rightCol->addLayout(liveRow);
    }

    rightCol->addSpacing(6);

    // Low / High freq spinboxes — eqform.cs:540-622 [v2.10.3.13].
    {
        QGridLayout* g = new QGridLayout;
        g->setHorizontalSpacing(6);
        g->setVerticalSpacing(4);

        g->addWidget(new QLabel(tr("Curve low"),  panel), 0, 0);
        m_paraLowSpin = new QDoubleSpinBox(panel);
        m_paraLowSpin->setObjectName(QStringLiteral("TxEqParaLowSpin"));
        m_paraLowSpin->setDecimals(3);
        m_paraLowSpin->setRange(kParaLowMinHz, kParaLowMaxHz);
        m_paraLowSpin->setValue(kParaLowDefaultHz);
        m_paraLowSpin->setSuffix(QStringLiteral(" Hz"));
        m_paraLowSpin->setToolTip(tr(
            "Lower edge of the configured curve range (Hz). Rescales all band frequencies.  "
            "Must be at least 1000 Hz below High."));
        g->addWidget(m_paraLowSpin, 0, 1);

        g->addWidget(new QLabel(tr("Curve high"), panel), 1, 0);
        m_paraHighSpin = new QDoubleSpinBox(panel);
        m_paraHighSpin->setObjectName(QStringLiteral("TxEqParaHighSpin"));
        m_paraHighSpin->setDecimals(3);
        m_paraHighSpin->setRange(kParaHighMinHz, kParaHighMaxHz);
        m_paraHighSpin->setValue(kParaHighDefaultHz);
        m_paraHighSpin->setSuffix(QStringLiteral(" Hz"));
        m_paraHighSpin->setToolTip(tr(
            "Upper edge of the configured curve range (Hz). Rescales all band frequencies.  "
            "Must be at least 1000 Hz above Low."));
        g->addWidget(m_paraHighSpin, 1, 1);

        rightCol->addLayout(g);
    }

    rightCol->addSpacing(6);

    // 5 / 10 / 18 band radios — eqform.cs:493-526 [v2.10.3.13]
    // (panelTS1 wraps the three radios; default 10-band).
    {
        QGroupBox* grp = new QGroupBox(tr("Bands"), panel);
        QHBoxLayout* gv = new QHBoxLayout(grp);
        gv->setContentsMargins(8, 14, 8, 8);
        gv->setSpacing(2);

        m_paraBands5Radio = new QRadioButton(tr("5-band"), grp);
        m_paraBands5Radio->setObjectName(QStringLiteral("TxEqParaBands5Radio"));
        m_paraBands5Radio->setToolTip(tr(
            "Switch parametric layout to 5 bands.  Resets per-band points."));

        m_paraBands10Radio = new QRadioButton(tr("10-band"), grp);
        m_paraBands10Radio->setObjectName(QStringLiteral("TxEqParaBands10Radio"));
        m_paraBands10Radio->setChecked(true);
        m_paraBands10Radio->setToolTip(tr(
            "Switch parametric layout to 10 bands (default).  "
            "Resets per-band points."));

        m_paraBands18Radio = new QRadioButton(tr("18-band"), grp);
        m_paraBands18Radio->setObjectName(QStringLiteral("TxEqParaBands18Radio"));
        m_paraBands18Radio->setToolTip(tr(
            "Switch parametric layout to 18 bands.  Resets per-band points."));

        m_bandCountGroup = new QButtonGroup(grp);
        m_bandCountGroup->addButton(m_paraBands5Radio,  5);
        m_bandCountGroup->addButton(m_paraBands10Radio, 10);
        m_bandCountGroup->addButton(m_paraBands18Radio, 18);

        gv->addWidget(m_paraBands5Radio);
        gv->addWidget(m_paraBands10Radio);
        gv->addWidget(m_paraBands18Radio);
        col->insertWidget(0, grp);
    }

    rightCol->addStretch(1);
    auto* guide = new QLabel(tr("Drag a point to change frequency and gain. Drag square handles to change width."), paraAdvanced);
    guide->setWordWrap(true); rightCol->addWidget(guide);


    col->addLayout(mainRow, 1);
    auto* controlsScroll = new QScrollArea(panel); controlsScroll->setWidgetResizable(true);
    controlsScroll->setObjectName(QStringLiteral("TxEqSelectedControlsScroll"));
    controlsScroll->setFrameShape(QFrame::NoFrame); controlsScroll->setMinimumHeight(100); controlsScroll->setMaximumHeight(260);
    auto* controls = new QWidget(controlsScroll); auto* details = new QVBoxLayout(controls);
    details->setContentsMargins(0, 0, 0, 0); details->setSpacing(6);
    controlsScroll->setWidget(controls); col->addWidget(controlsScroll);
    auto* bandScroll = new QScrollArea(panel); bandScroll->setWidgetResizable(false);
    bandScroll->setObjectName(QStringLiteral("TxEqBandStripScroll"));
    bandScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    bandScroll->setFrameShape(QFrame::NoFrame); bandScroll->setFixedHeight(65);
    auto* bandButtons = new QWidget(bandScroll); m_bandSelectorRow = new QHBoxLayout(bandButtons);
    m_bandSelectorRow->setContentsMargins(0, 0, 0, 0); m_bandSelectorRow->setSpacing(4);
    m_bandSelector = new QButtonGroup(this); m_bandSelector->setObjectName(QStringLiteral("TxEqBandSelector"));
    bandScroll->setWidget(bandButtons); details->addWidget(bandScroll);
    m_selectedLabel = new QLabel(panel); m_selectedLabel->setObjectName(QStringLiteral("TxEqSelectedBandLabel"));
    details->addWidget(m_selectedLabel);
    // Arrange selected-band values in a grid so laptop widths remain usable.
    auto* editorGrid = new QGridLayout;
    int field = 0;
    while (editRow->count()) {
        auto* item = editRow->takeAt(0);
        if (auto* widget = item->widget()) {
            if (auto* label = qobject_cast<QLabel*>(widget)) {
                if (label->text() == tr("Band")) { label->hide(); }
                else { editorGrid->addWidget(label, 0, field++); }
            } else if (widget == m_paraResetBtn) { editorGrid->addWidget(widget, 2, 3); }
            else { editorGrid->addWidget(widget, 1, field - 1); }
        }
        delete item;
    }
    delete editRow;
    details->addLayout(editorGrid);
    auto* widthRow = new QHBoxLayout;
    widthRow->addWidget(m_paraUseQFactorsChk);
    widthRow->addWidget(new QLabel(tr("Wider"), panel));
    m_widthSlider = new QSlider(Qt::Horizontal, panel); m_widthSlider->setRange(0, 1000);
    m_widthSlider->setObjectName(QStringLiteral("TxEqWidthSlider"));
    m_widthSlider->setAccessibleName(tr("Selected band width Q"));
    m_widthSlider->setStyleSheet(Style::sliderHStyle());
    widthRow->addWidget(m_widthSlider, 1); widthRow->addWidget(new QLabel(tr("Narrower"), panel));
    details->addLayout(widthRow);
    auto* limitation = new QLabel(tr("TX Width: 10-band audio does not use Q; 5/18-band audio uses a ten-point approximation."), panel);
    limitation->setObjectName(QStringLiteral("TxEqWidthLimitation")); limitation->setWordWrap(true);
    details->addWidget(limitation); m_paraQSpin->setToolTip(limitation->text()); m_widthSlider->setToolTip(limitation->text());
    m_countNotice = new QWidget(panel); auto* noticeRow = new QHBoxLayout(m_countNotice);
    m_countMessage = new QLabel(m_countNotice); m_countMessage->setWordWrap(true); noticeRow->addWidget(m_countMessage, 1);
    auto* apply = new QPushButton(tr("Apply"), m_countNotice); apply->setObjectName(QStringLiteral("TxEqCountApplyBtn"));
    auto* cancel = new QPushButton(tr("Cancel"), m_countNotice); cancel->setObjectName(QStringLiteral("TxEqCountCancelBtn"));
    apply->setAutoDefault(false); cancel->setAutoDefault(false); noticeRow->addWidget(apply); noticeRow->addWidget(cancel);
    connect(apply, &QPushButton::clicked, this, &TxEqDialog::applyBandCount);
    connect(cancel, &QPushButton::clicked, this, &TxEqDialog::cancelBandCount);
    col->insertWidget(1, m_countNotice); m_countNotice->hide();
    m_parametricWidget->setSelectedIndex(0);
    rebuildBandSelectors();
    return root;
}

// ─────────────────────────────────────────────────────────────────────
// Signal wiring
// ─────────────────────────────────────────────────────────────────────

void TxEqDialog::wireSignals()
{
    // ── chkLegacyEQ toggle — From eqform.cs:981 + 2862-2911 [v2.10.3.13].
    connect(m_modeSelector, &QButtonGroup::idClicked, this, [this](int id) { onLegacyToggled(id == 0); });
    connect(m_undoBtn, &QPushButton::clicked, this, &TxEqDialog::undoEdit);
    connect(m_redoBtn, &QPushButton::clicked, this, &TxEqDialog::redoEdit);
    for (auto* history : m_history) {
        connect(history, &EqEditHistory::availabilityChanged, this, [this] { refreshHistoryButtons(); });
    }
    for (auto* spin : findChildren<QAbstractSpinBox*>()) {
        spin->setKeyboardTracking(false); spin->installEventFilter(this);
        spin->setAccessibleName(spin->toolTip().isEmpty() ? spin->objectName() : spin->toolTip());
        for (auto* line : spin->findChildren<QLineEdit*>()) { line->installEventFilter(this); }
        connect(spin, &QAbstractSpinBox::editingFinished, this, [this, spin] {
            if (m_numericEditor == spin) { m_numericEditor = nullptr; finishEdit(); }
        });
    }
    for (auto* slider : findChildren<QSlider*>()) {
        slider->installEventFilter(this); slider->setAccessibleName(slider->toolTip().isEmpty() ? slider->objectName() : slider->toolTip());
        connect(slider, &QSlider::sliderPressed, this, [this] { beginEdit(); m_sliderActive = true; });
        connect(slider, &QSlider::sliderReleased, this, [this] { m_sliderActive = false; finishEdit(); });
    }
    connect(m_widthSlider, &QSlider::valueChanged, this, [this](int value) { onParametricQSpinChanged(.2 * std::pow(100.0, value / 1000.0)); });
    connect(m_bandSelector, &QButtonGroup::idClicked, this, [this](int id) {
        m_parametricWidget->setSelectedIndex(m_parametricWidget->getIndexFromBandId(id)); updateEditRowFromSelection();
    });
    connect(m_parametricWidget, &ParametricEqWidget::editStarted, this, [this] { beginEdit(); m_gestureActive = true; });
    connect(m_parametricWidget, &ParametricEqWidget::editFinished, this, [this] { m_gestureActive = false; finishEdit(); });

    // ── Legacy panel: UI → model ────────────────────────────────────
    connect(m_enableChk, &QCheckBox::toggled,
            this, &TxEqDialog::onEnableToggled);

    // Slider/spin pair for preamp — use shared onPreampChanged.
    connect(m_preampSlider, &QSlider::valueChanged,
            this, &TxEqDialog::onPreampChanged);
    connect(m_preampSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &TxEqDialog::onPreampChanged);

    for (int i = 0; i < 10; ++i) {
        connect(m_bandSliders[i], &QSlider::valueChanged,
                this, &TxEqDialog::onBandValueChanged);
        connect(m_bandSpins[i], qOverload<int>(&QSpinBox::valueChanged),
                this, &TxEqDialog::onBandValueChanged);
        connect(m_freqSpins[i], qOverload<int>(&QSpinBox::valueChanged),
                this, &TxEqDialog::onFreqValueChanged);
    }

    connect(m_ncSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &TxEqDialog::onNcChanged);
    connect(m_mpChk, &QCheckBox::toggled,
            this, &TxEqDialog::onMpToggled);
    connect(m_ctfmodeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &TxEqDialog::onCtfmodeChanged);
    connect(m_wintypeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &TxEqDialog::onWintypeChanged);

    // ── Parametric panel wiring ─────────────────────────────────────
    if (m_parametricWidget) {
        connect(m_parametricWidget, &ParametricEqWidget::pointsChanged,
                this, &TxEqDialog::onParametricPointsChanged);
        connect(m_parametricWidget, &ParametricEqWidget::globalGainChanged,
                this, &TxEqDialog::onParametricGlobalGainChanged);
        connect(m_parametricWidget, &ParametricEqWidget::selectedIndexChanged,
                this, &TxEqDialog::onParametricSelectedChanged);
    }
    connect(m_paraResetBtn, &QPushButton::clicked,
            this, &TxEqDialog::onParametricResetClicked);
    connect(m_bandCountGroup, &QButtonGroup::idToggled,
            this, [this](int /*id*/, bool checked) {
        if (checked) onParametricBandCountChanged();
    });
    connect(m_paraLowSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &TxEqDialog::onParametricLowFreqChanged);
    connect(m_paraHighSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &TxEqDialog::onParametricHighFreqChanged);
    connect(m_paraLogScaleChk, &QCheckBox::toggled,
            this, &TxEqDialog::onParametricLogScaleToggled);
    connect(m_paraUseQFactorsChk, &QCheckBox::toggled,
            this, &TxEqDialog::onParametricUseQFactorsToggled);
    connect(m_paraLiveUpdateChk, &QCheckBox::toggled,
            this, &TxEqDialog::onParametricLiveUpdateToggled);
    connect(m_paraFreqSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &TxEqDialog::onParametricFreqSpinChanged);
    connect(m_paraGainSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &TxEqDialog::onParametricGainSpinChanged);
    connect(m_paraQSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &TxEqDialog::onParametricQSpinChanged);
    connect(m_paraPreampSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &TxEqDialog::onParametricPreampSpinChanged);

    // ── Model → UI ────────────────────────────────────────────────
    if (!m_radio) {
        return;
    }
    TransmitModel& tx = m_radio->transmitModel();

    connect(&tx, &TransmitModel::txEqEnabledChanged,
            this, &TxEqDialog::syncFromModel);
    connect(&tx, &TransmitModel::txEqPreampChanged,
            this, &TxEqDialog::syncFromModel);
    connect(&tx, &TransmitModel::txEqBandChanged,
            this, &TxEqDialog::syncFromModel);
    connect(&tx, &TransmitModel::txEqFreqChanged,
            this, &TxEqDialog::syncFromModel);
    connect(&tx, &TransmitModel::txEqNcChanged,
            this, &TxEqDialog::syncFromModel);
    connect(&tx, &TransmitModel::txEqMpChanged,
            this, &TxEqDialog::syncFromModel);
    connect(&tx, &TransmitModel::txEqCtfmodeChanged,
            this, &TxEqDialog::syncFromModel);
    connect(&tx, &TransmitModel::txEqWintypeChanged,
            this, &TxEqDialog::syncFromModel);

    // Codex P1 #2 on PR #159: profile activation fires
    // txEqParaEqDataChanged; without this connect, the parametric widget
    // stays at defaults after a profile load and the next user edit
    // overwrites the just-loaded curve.
    connect(&tx, &TransmitModel::txEqParaEqDataChanged,
            this, &TxEqDialog::syncParametricFromModel);
    if (auto* profiles = m_radio->micProfileManager()) {
        // Activation is authoritative even if every scalar/blob setter is
        // idempotent: cancel unsaved runtime edits and seed from saved data.
        connect(profiles, &MicProfileManager::activeProfileChanged, this, [this] { syncFromModel(); });
    }
}

// ─────────────────────────────────────────────────────────────────────
// User-driven slots — Legacy panel
// ─────────────────────────────────────────────────────────────────────

void TxEqDialog::onEnableToggled(bool on)
{
    if (m_updatingFromModel || !m_radio) { return; }
    const QScopedValueRollback<bool> guard(m_updatingFromModel, true);
    m_radio->transmitModel().setTxEqEnabled(on);
}

void TxEqDialog::onPreampChanged(int dB)
{
    if (m_updatingFromModel || !m_radio) { return; }
    { QSignalBlocker a(m_preampSlider), b(m_preampSpin); m_preampSlider->setValue(dB); m_preampSpin->setValue(dB); }
    changed();
}

void TxEqDialog::onBandValueChanged()
{
    if (m_updatingFromModel || !m_radio) { return; }
    QObject* s = sender();
    if (!s) { return; }
    bool ok = false;
    const int idx = s->property(kBandIndexProp).toInt(&ok);
    if (!ok || idx < 0 || idx >= 10) { return; }
    int value = 0;
    if (auto* slider = qobject_cast<QSlider*>(s)) {
        value = slider->value();
    } else if (auto* spin = qobject_cast<QSpinBox*>(s)) {
        value = spin->value();
    } else {
        return;
    }
    { QSignalBlocker a(m_bandSliders[idx]), b(m_bandSpins[idx]); m_bandSliders[idx]->setValue(value); m_bandSpins[idx]->setValue(value); }
    changed();
}

void TxEqDialog::onFreqValueChanged()
{
    if (m_updatingFromModel || !m_radio) { return; }
    QObject* s = sender();
    if (!s) { return; }
    bool ok = false;
    const int idx = s->property(kBandIndexProp).toInt(&ok);
    if (!ok || idx < 0 || idx >= 10) { return; }
    auto* spin = qobject_cast<QSpinBox*>(s);
    if (!spin) { return; }
    changed();
}

void TxEqDialog::onNcChanged(int /*nc*/)
{
    if (m_updatingFromModel || !m_radio) { return; }
    changed();
}

void TxEqDialog::onMpToggled(bool /*mp*/)
{
    if (m_updatingFromModel || !m_radio) { return; }
    changed();
}

void TxEqDialog::onCtfmodeChanged(int /*mode*/)
{
    if (m_updatingFromModel || !m_radio) { return; }
    changed();
}

void TxEqDialog::onWintypeChanged(int /*wintype*/)
{
    if (m_updatingFromModel || !m_radio) { return; }
    changed();
}

// ─────────────────────────────────────────────────────────────────────
// Legacy <-> Parametric panel toggle.
// From Thetis chkLegacyEQ_CheckedChanged at eqform.cs:2862-2911 [v2.10.3.13].
// We don't need to swap WDSP DSPRX paths (Thetis cs:2869-2871) — that
// belongs to the DSP layer, not to this TX-only dialog.  We only flip
// the visible panel and persist the user choice for the next launch.
// ─────────────────────────────────────────────────────────────────────

void TxEqDialog::onLegacyToggled(bool legacy)
{
    if (!m_panelStack || legacy == usingLegacyEq()) { return; }
    if (m_numericEditor || m_gestureActive || m_sliderActive) { m_numericEditor = nullptr; m_gestureActive = false; m_sliderActive = false; finishEdit(); }
    m_parametricWidget->cancelEditGesture(); cancelBandCount();
    { QSignalBlocker b(m_modeSelector); m_modeSelector->button(legacy ? 0 : 1)->setChecked(true); }
    m_panelStack->setCurrentIndex(legacy ? 0 : 1);
    m_committed[legacy ? 0 : 1] = captureEditState(legacy);
    refreshHistoryButtons();
    if (auto* advanced = findChild<QWidget*>(QStringLiteral("TxEqParaAdvancedControls"))) { advanced->setVisible(!legacy); }
    AppSettings::instance().setValue(
        QLatin1String(kLegacyToggleSettingsKey),
        legacy ? QStringLiteral("True") : QStringLiteral("False"));

    // Push the active mode's curve to WDSP immediately so toggling
    // alone takes audible effect (without this the user would have
    // to also nudge a control on the newly-active panel before the
    // audio path picked up the curve).  See the
    // pushParametricCurveToWdsp / pushLegacyCurveToWdsp comments for
    // why each helper was needed.
    if (legacy) {
        pushLegacyCurveToWdsp();
    } else {
        pushParametricCurveToWdsp();
    }
}

// ─────────────────────────────────────────────────────────────────────
// Parametric panel slots
// ─────────────────────────────────────────────────────────────────────

void TxEqDialog::onParametricPointsChanged(bool isDragging)
{
    if (m_ignoreUpdates || m_updatingFromModel) { return; }
    updateEditRowFromSelection();
    if (!m_gestureActive || isDragging) { changed(isDragging); }
}

void TxEqDialog::onParametricGlobalGainChanged(bool isDragging)
{
    if (m_ignoreUpdates || m_updatingFromModel) { return; }
    updateEditRowFromSelection();
    if (!m_gestureActive || isDragging) { changed(isDragging); }
}

void TxEqDialog::onParametricSelectedChanged(bool /*isDragging*/)
{
    if (m_ignoreUpdates) { return; }
    updateEditRowFromSelection();
}

void TxEqDialog::onParametricResetClicked()
{
    if (!m_parametricWidget) { return; }
    // Mirrors btnParaEQReset_Click at eqform.cs:731-740 (calls
    // ucParametricEq1.SetDefaults).  NereusSDR's ParametricEqWidget keeps
    // resetPointsDefault() private (Task 5 review), so we synthesize the
    // flat-default arrays inline and call setPointsData -- same pattern as
    // TxCfcDialog::onResetCompClicked / onResetEqClicked.  setBandCount()
    // can't be reused as the reset hook because it early-returns when the
    // requested count equals the current count.
    beginEdit();
    const int bands = m_parametricWidget->bandCount();
    QVector<double> f(bands), g(bands, 0.0), q(bands, 4.0);
    const double minHz = m_parametricWidget->frequencyMinHz();
    const double maxHz = m_parametricWidget->frequencyMaxHz();
    const double span = (maxHz > minHz) ? (maxHz - minHz) : 1.0;
    for (int i = 0; i < bands; ++i) {
        const double t = (bands > 1) ? double(i) / double(bands - 1) : 0.0;
        f[i] = minHz + t * span;
    }
    {
        QSignalBlocker b(m_parametricWidget);
        m_parametricWidget->setSelectedIndex(0);
        m_parametricWidget->setGlobalGainDb(0.0);
        m_parametricWidget->setPointsData(f, g, q);
    }
    updateEditRowFromSelection();
    changed();
}

void TxEqDialog::onParametricBandCountChanged()
{
    const int count = m_bandCountGroup->checkedId();
    if (count == m_parametricWidget->bandCount()) { cancelBandCount(); return; }
    m_pendingCount = count;
    m_countMessage->setText(tr("Changing to %1 bands resets band frequencies, gains and widths. Apply this change?").arg(count));
    m_countNotice->show();
}

void TxEqDialog::onParametricLowFreqChanged(double hz)
{
    if (!m_parametricWidget || !m_paraHighSpin) { return; }
    // Enforce the 1 kHz spread guard — clamp Low so it stays at least
    // kMinFreqSpreadHz below High.
    const double hi = m_parametricWidget->frequencyMaxHz();
    if (hz + kMinFreqSpreadHz > hi) {
        QSignalBlocker b(m_paraLowSpin);
        hz = hi - kMinFreqSpreadHz;
        m_paraLowSpin->setValue(hz);
    }
    if (!m_numericEditor && !m_sliderActive && !m_gestureActive) { beginEdit(); }
    { QSignalBlocker b(m_parametricWidget); m_parametricWidget->setFrequencyMinHz(static_cast<double>(hz)); }
    changed();
}

void TxEqDialog::onParametricHighFreqChanged(double hz)
{
    if (!m_parametricWidget || !m_paraLowSpin) { return; }
    const double lo = m_parametricWidget->frequencyMinHz();
    if (hz < lo + kMinFreqSpreadHz) {
        QSignalBlocker b(m_paraHighSpin);
        hz = lo + kMinFreqSpreadHz;
        m_paraHighSpin->setValue(hz);
    }
    if (!m_numericEditor && !m_sliderActive && !m_gestureActive) { beginEdit(); }
    { QSignalBlocker b(m_parametricWidget); m_parametricWidget->setFrequencyMaxHz(static_cast<double>(hz)); }
    changed();
}

void TxEqDialog::onParametricLogScaleToggled(bool on)
{
    if (!m_parametricWidget) { return; }
    if (!m_numericEditor && !m_sliderActive && !m_gestureActive) { beginEdit(); }
    { QSignalBlocker b(m_parametricWidget); m_parametricWidget->setLogScale(on); }
    changed();
}

void TxEqDialog::onParametricUseQFactorsToggled(bool on)
{
    if (!m_parametricWidget) { return; }
    if (!m_numericEditor && !m_sliderActive && !m_gestureActive) { beginEdit(); }
    { QSignalBlocker b(m_parametricWidget); m_parametricWidget->setParametricEq(on); }
    updateEditRowFromSelection();
    changed();
}

void TxEqDialog::onParametricLiveUpdateToggled(bool /*on*/)
{
    // Existing display preference: read while editing, with no audio write.
}

void TxEqDialog::onParametricFreqSpinChanged(int hz)
{
    if (m_ignoreUpdates || !m_parametricWidget) { return; }
    const int idx = m_parametricWidget->selectedIndex();
    if (idx < 0) { return; }
    if (!m_numericEditor && !m_sliderActive && !m_gestureActive) { beginEdit(); }
    double f = 0.0, g = 0.0, q = 0.0;
    const auto point = m_parametricWidget->points()[idx];
    f = point.frequencyHz; g = point.gainDb; q = point.q;
    f = static_cast<double>(hz);
    {
        QSignalBlocker b(m_parametricWidget);
        m_parametricWidget->setPointData(idx, f, g, q);
    }
    updateEditRowFromSelection(); changed();
}

void TxEqDialog::onParametricGainSpinChanged(double db)
{
    changeSelectedPoint(db, false);
}

void TxEqDialog::onParametricQSpinChanged(double q)
{
    changeSelectedPoint(q, true);
}

void TxEqDialog::onParametricPreampSpinChanged(double db)
{
    if (m_ignoreUpdates || !m_parametricWidget) { return; }
    if (!m_numericEditor && !m_sliderActive && !m_gestureActive) { beginEdit(); }
    {
        QSignalBlocker b(m_parametricWidget);
        m_parametricWidget->setGlobalGainDb(db);
    }
    changed();
}

// ─────────────────────────────────────────────────────────────────────
// Edit-row sync helpers
// ─────────────────────────────────────────────────────────────────────

void TxEqDialog::updateEditRowFromSelection()
{
    if (!m_parametricWidget) { return; }
    const int idx = m_parametricWidget->selectedIndex();
    const bool haveSelection = (idx >= 0);
    rebuildBandSelectors();
    { QSignalBlocker b(m_paraPreampSpin); m_paraPreampSpin->setValue(m_parametricWidget->globalGainDb()); }
    const bool useQ = m_parametricWidget->parametricEq();
    m_widthSlider->setEnabled(haveSelection && useQ);

    // Edit-row spinboxes are enabled only when a band is selected,
    // matching Thetis frmCFCConfig.cs's selected-row-enable pattern
    // (we have no Thetis equivalent in eqform; Thetis leaves the
    // boxes unconditionally editable).  NereusSDR-spin: gate them
    // so users don't type into spinboxes that have no effect.
    if (m_paraFreqSpin)   m_paraFreqSpin->setEnabled(haveSelection);
    if (m_paraGainSpin)   m_paraGainSpin->setEnabled(haveSelection);
    if (m_paraQSpin)      m_paraQSpin->setEnabled(haveSelection && useQ);

    if (!haveSelection) {
        return;
    }

    double f = 0.0, g = 0.0, q = 0.0;
    const auto point = m_parametricWidget->points()[idx];
    f = point.frequencyHz; g = point.gainDb; q = point.q;

    m_selectedLabel->setText(tr("Band %1 · %2 Hz").arg(point.bandId).arg(f, 0, 'f', 0));
    { QSignalBlocker b(m_widthSlider); m_widthSlider->setValue(qRound(1000 * std::log(q / .2) / std::log(100.0))); }
    m_ignoreUpdates = true;
    if (m_paraFreqSpin) {
        QSignalBlocker b(m_paraFreqSpin);
        m_paraFreqSpin->setValue(static_cast<int>(std::round(f)));
    }
    if (m_paraGainSpin) {
        QSignalBlocker b(m_paraGainSpin);
        m_paraGainSpin->setValue(g);
    }
    if (m_paraQSpin) {
        QSignalBlocker b(m_paraQSpin);
        m_paraQSpin->setValue(q);
    }
    if (m_paraPreampSpin) {
        QSignalBlocker b(m_paraPreampSpin);
        m_paraPreampSpin->setValue(m_parametricWidget->globalGainDb());
    }
    m_ignoreUpdates = false;
}

void TxEqDialog::pushParametricToModel()
{
    if (!m_radio || !m_parametricWidget || m_updatingFromModel) { return; }
    m_updatingFromModel = true;
    TransmitModel& tx = m_radio->transmitModel();
    tx.setTxEqNc(m_ncSpin->value()); tx.setTxEqMp(m_mpChk->isChecked());
    tx.setTxEqCtfmode(m_ctfmodeCombo->currentIndex()); tx.setTxEqWintype(m_wintypeCombo->currentIndex());

    // 1. Persist the parametric blob (consumed by profile save / load
    //    and by syncParametricFromModel on the next active-profile flip).
    // From Thetis eqform.cs:3254-3256 + Common.cs:1745-1762
    // [v2.10.3.13] — SaveToJsonFromPoints(...) then Compress_gzip(json).
    tx.setTxEqParaEqData(
        ParaEqEnvelope::encode(m_parametricWidget->saveToJson()));

    // 2. Push the parametric curve directly to WDSP via
    //    TxChannel::setTxEqProfile (Codex P1 #1 on PR #159 + the
    //    follow-up self-review of dd03b70).  See
    //    pushParametricCurveToWdsp for the F[10]/G[11] build rules.
    //
    //    Why NOT push via the legacy txEqBand/txEqFreq/txEqPreamp
    //    setter chain (the dd03b70 approach):
    //      - rounding to int loses parametric precision (4.6 dB -> 5)
    //      - sampling at the LEGACY ISO grid discards the user's
    //        chosen parametric band centers
    //      - mutating tx.txEqBand[] etc. corrupts the user's legacy
    //        settings on toggle-back
    //      - 11 emissions per drag triggers 11 WDSP profile rebuilds
    //        when only one is needed
    //    Direct push avoids all four.
    pushParametricCurveToWdsp();

    m_updatingFromModel = false;
}

// Build the WDSP (F[10], G[11]) shape from the parametric widget's
// current state and push via TxChannel::setTxEqProfile.  Called on
// every parametric edit (from pushParametricToModel) and on toggle
// INTO parametric mode (from onLegacyToggled).
//
// WDSP TX EQ has a fixed 10-band shape; the parametric widget has
// 5/10/18 bands with arbitrary centers + Q.  This helper resolves
// the count mismatch:
//   - 10-band parametric: 1:1 push of (freq, gain) -- parametric
//     peaks land at the user's exact frequencies.
//   - 5/18-band parametric: sample the curve at 10 equally-spaced
//     freqs across the parametric range -- the curve's Gaussian-
//     weighted-sum interpolation captures the Q effect at whatever
//     resolution 10 sample points allow.
// This is NereusSDR's existing compatibility conversion, not current
// Thetis behavior. Thetis eqform.cs:3041-3070 [v2.10.3.15] sends actual
// configured F/G/Q arrays to its newer five-argument WDSP setter.
// Bundled NereusSDR WDSP remains ten-band F/G: Q has no audio effect
// at count 10; counts 5/18 retain only its ten-point approximation.
void TxEqDialog::pushParametricCurveToWdsp()
{
    if (!m_radio || !m_parametricWidget) { return; }
    auto* ch = m_radio->txChannel();
    if (!ch) { return; }

    std::vector<double> freqs(10);
    std::vector<double> gains(11);
    gains[0] = m_parametricWidget->globalGainDb();  // preamp slot

    const int n = m_parametricWidget->bandCount();
    if (n == 10) {
        for (int i = 0; i < 10; ++i) {
            double f = 0.0, g = 0.0, q = 0.0;
            m_parametricWidget->getPointData(i, f, g, q);
            freqs[i]   = f;
            gains[i+1] = g;
        }
    } else {
        const double minHz = m_parametricWidget->frequencyMinHz();
        const double maxHz = m_parametricWidget->frequencyMaxHz();
        const double step  = (maxHz > minHz) ? (maxHz - minHz) / 9.0 : 0.0;
        for (int i = 0; i < 10; ++i) {
            const double f = minHz + step * i;
            freqs[i]   = f;
            gains[i+1] = m_parametricWidget->responseDbAtFrequency(f);
        }
    }
    ch->setTxEqProfile(freqs, gains);
}

// Build the WDSP (F[10], G[11]) shape from the legacy
// txEqFreq/txEqBand/txEqPreamp model fields and push via
// TxChannel::setTxEqProfile.  Called on toggle BACK to legacy mode
// so WDSP restores the legacy curve immediately.
//
// Without this helper, the legacy-panel sliders' per-band setters
// only push to WDSP on user edit (via RadioModel.cpp:1924-1939's
// pushEqProfile lambda).  Toggling legacy with no edit would leave
// the previous parametric curve on WDSP until the user nudged a
// slider -- a confusing UX dead zone.
void TxEqDialog::pushLegacyCurveToWdsp()
{
    if (!m_radio) { return; }
    auto* ch = m_radio->txChannel();
    if (!ch) { return; }
    TransmitModel& tx = m_radio->transmitModel();

    std::vector<double> freqs(10);
    std::vector<double> gains(11);
    gains[0] = static_cast<double>(tx.txEqPreamp());
    for (int i = 0; i < 10; ++i) {
        freqs[i]   = static_cast<double>(tx.txEqFreq(i));
        gains[i+1] = static_cast<double>(tx.txEqBand(i));
    }
    ch->setTxEqProfile(freqs, gains);
}

// Codex P1 #2 on PR #159: profile activation fires
// txEqParaEqDataChanged on the model, but we previously only subscribed
// to legacy TX EQ signals -- so the parametric widget would stay at
// defaults after profile load and the next user edit (which goes
// through pushParametricToModel) would overwrite the just-loaded curve.
// This slot loads the JSON blob into the widget under a signal blocker
// so the load itself doesn't trigger a feedback push back to the model.
void TxEqDialog::syncParametricFromModel()
{
    if (!m_radio || !m_parametricWidget || m_updatingFromModel) { return; }
    const QString blob = m_radio->transmitModel().txEqParaEqData();
    const bool log = m_parametricWidget->logScale();
    {
        const QScopedValueRollback<bool> guard(m_updatingFromModel, true);
        QSignalBlocker graph(m_parametricWidget), low(m_paraLowSpin), high(m_paraHighSpin), q(m_paraUseQFactorsChk), logarithmic(m_paraLogScaleChk);
        m_parametricWidget->cancelEditGesture();
        m_parametricWidget->restoreEditState(m_seedGraph);
        m_parametricWidget->setLogScale(log);
        // Preserve Thetis envelope and early raw-JSON compatibility. Unknown
        // or empty profiles seed the same existing defaults as a fresh dialog,
        // while the original opaque blob remains untouched until an edit.
        const auto decoded = ParaEqEnvelope::decode(blob);
        const QString json = decoded.has_value() ? *decoded : (blob.trimmed().startsWith(QLatin1Char('{')) ? blob : QString());
        if (!json.isEmpty() && !m_parametricWidget->loadFromJson(json)) {
            m_parametricWidget->restoreEditState(m_seedGraph); m_parametricWidget->setLogScale(log);
        }
        m_parametricWidget->setSelectedIndex(0);
        m_paraLowSpin->setValue(m_parametricWidget->frequencyMinHz());
        m_paraHighSpin->setValue(m_parametricWidget->frequencyMaxHz());
        m_paraUseQFactorsChk->setChecked(m_parametricWidget->parametricEq());
        m_paraLogScaleChk->setChecked(m_parametricWidget->logScale());
        updateEditRowFromSelection();
    }
    m_loadedBlob = blob; m_loadedGraph = m_parametricWidget->saveEditState();
    rebaseEditHistory();
}

// ─────────────────────────────────────────────────────────────────────
// Model → UI sync (echo-guarded)
// ─────────────────────────────────────────────────────────────────────

void TxEqDialog::syncFromModel()
{
    if (!m_radio || m_updatingFromModel) { return; }
    m_parametricWidget->cancelEditGesture();
    TransmitModel& tx = m_radio->transmitModel();

    m_updatingFromModel = true;

    {
        QSignalBlocker b(m_enableChk);
        m_enableChk->setChecked(tx.txEqEnabled());
    }
    {
        QSignalBlocker bs(m_preampSlider);
        QSignalBlocker bn(m_preampSpin);
        m_preampSlider->setValue(tx.txEqPreamp());
        m_preampSpin->setValue(tx.txEqPreamp());
    }
    for (int i = 0; i < 10; ++i) {
        {
            QSignalBlocker bs(m_bandSliders[i]);
            QSignalBlocker bn(m_bandSpins[i]);
            m_bandSliders[i]->setValue(tx.txEqBand(i));
            m_bandSpins[i]->setValue(tx.txEqBand(i));
        }
        {
            QSignalBlocker bf(m_freqSpins[i]);
            m_freqSpins[i]->setValue(tx.txEqFreq(i));
        }
    }
    {
        QSignalBlocker b(m_ncSpin);
        m_ncSpin->setValue(tx.txEqNc());
    }
    {
        QSignalBlocker b(m_mpChk);
        m_mpChk->setChecked(tx.txEqMp());
    }
    {
        QSignalBlocker b(m_ctfmodeCombo);
        m_ctfmodeCombo->setCurrentIndex(tx.txEqCtfmode());
    }
    {
        QSignalBlocker b(m_wintypeCombo);
        m_wintypeCombo->setCurrentIndex(tx.txEqWintype());
    }

    m_updatingFromModel = false;

    // Hydrate the parametric widget from the stored JSON blob (Codex P1 #2
    // on PR #159).  Initial profile-load path -- subsequent updates fire
    // via the txEqParaEqDataChanged signal wired in wireSignals().
    syncParametricFromModel();
    rebaseEditHistory();
}

// ─────────────────────────────────────────────────────────────────────
// Hide-on-close — From Thetis frmCFCConfig.cs:477-482 [v2.10.3.13]
// pattern.  TxApplet keeps the singleton alive for fast re-show.
// ─────────────────────────────────────────────────────────────────────

// NereusSDR-original editor transaction plumbing. Runtime snapshots preserve
// doubles and stable IDs; the existing saved JSON and WDSP conversion stay intact.
bool TxEqDialog::usingLegacyEq() const
{
    return m_panelStack && m_panelStack->currentIndex() == 0;
}

QByteArray TxEqDialog::captureEditState(bool legacy) const
{
    QByteArray result; QDataStream stream(&result, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << m_ncSpin->value() << m_mpChk->isChecked() << m_ctfmodeCombo->currentIndex() << m_wintypeCombo->currentIndex();
    if (legacy) {
        stream << m_preampSpin->value();
        for (int i = 0; i < 10; ++i) { stream << m_bandSpins[i]->value() << m_freqSpins[i]->value(); }
    } else {
        stream << m_parametricWidget->saveEditState();
    }
    return result;
}

void TxEqDialog::restoreEditState(bool legacy, const QByteArray& state)
{
    QDataStream stream(state); stream.setVersion(QDataStream::Qt_6_0);
    int nc = 0, cutoff = 0, window = 0; bool mp = false;
    stream >> nc >> mp >> cutoff >> window;
    {
        const QScopedValueRollback<bool> guard(m_ignoreUpdates, true);
        QSignalBlocker a(m_ncSpin), b(m_mpChk), c(m_ctfmodeCombo), d(m_wintypeCombo);
        m_ncSpin->setValue(nc); m_mpChk->setChecked(mp); m_ctfmodeCombo->setCurrentIndex(cutoff); m_wintypeCombo->setCurrentIndex(window);
        if (legacy) {
            int preamp = 0; stream >> preamp;
            QSignalBlocker a(m_preampSlider), b(m_preampSpin);
            m_preampSlider->setValue(preamp); m_preampSpin->setValue(preamp);
            for (int i = 0; i < 10; ++i) {
                int gain = 0, hz = 0; stream >> gain >> hz;
                QSignalBlocker a(m_bandSliders[i]), b(m_bandSpins[i]), c(m_freqSpins[i]);
                m_bandSliders[i]->setValue(gain); m_bandSpins[i]->setValue(gain); m_freqSpins[i]->setValue(hz);
            }
        } else {
            QByteArray graph; stream >> graph;
            QSignalBlocker a(m_parametricWidget), b(m_paraLiveUpdateChk), c(m_paraLowSpin), d(m_paraHighSpin), e(m_paraLogScaleChk), f(m_paraUseQFactorsChk);
            if (!m_parametricWidget->restoreEditState(graph)) { return; }
            m_parametricWidget->setSelectedIndex(m_parametricWidget->getIndexFromBandId(m_selectionStates.value(state, -1)));
            m_paraLowSpin->setValue(m_parametricWidget->frequencyMinHz()); m_paraHighSpin->setValue(m_parametricWidget->frequencyMaxHz());
            m_paraLogScaleChk->setChecked(m_parametricWidget->logScale()); m_paraUseQFactorsChk->setChecked(m_parametricWidget->parametricEq());
        }
    }
    if (stream.status() != QDataStream::Ok || !stream.atEnd()) { return; }
    cancelBandCount(); updateEditRowFromSelection();
    m_committed[legacy ? 0 : 1] = state;
    if (legacy) { pushLegacyControls(); }
    else if (m_radio && m_parametricWidget->saveEditState() == m_loadedGraph) {
        const QScopedValueRollback<bool> guard(m_updatingFromModel, true);
        auto& tx = m_radio->transmitModel();
        tx.setTxEqNc(m_ncSpin->value()); tx.setTxEqMp(m_mpChk->isChecked());
        tx.setTxEqCtfmode(m_ctfmodeCombo->currentIndex()); tx.setTxEqWintype(m_wintypeCombo->currentIndex());
        tx.setTxEqParaEqData(m_loadedBlob);
        pushParametricCurveToWdsp();
    } else { pushParametricToModel(); }
    refreshHistoryButtons();
}

void TxEqDialog::beginEdit()
{
    if (m_ignoreUpdates || m_updatingFromModel) { return; }
    const bool legacy = usingLegacyEq(); const int mode = legacy ? 0 : 1;
    const QByteArray state = captureEditState(legacy);
    const int index = m_parametricWidget->selectedIndex();
    if (!legacy) { m_selectionStates.insert(state, index >= 0 ? m_parametricWidget->points()[index].bandId : -1); }
    m_history[mode]->beginEdit(state);
}

void TxEqDialog::finishEdit()
{
    if (m_ignoreUpdates || m_updatingFromModel) { return; }
    const bool legacy = usingLegacyEq(); const int mode = legacy ? 0 : 1;
    const QByteArray state = captureEditState(legacy);
    const bool edited = state != m_committed[mode];
    const int index = m_parametricWidget->selectedIndex();
    if (!legacy) { m_selectionStates.insert(state, index >= 0 ? m_parametricWidget->points()[index].bandId : -1); }
    if (edited) { m_history[mode]->commitEdit(state); } else { m_history[mode]->cancelEdit(); }
    m_committed[mode] = state;
    const auto retained = m_history[1]->retainedStates();
    for (auto it = m_selectionStates.begin(); it != m_selectionStates.end();) {
        if (!retained.contains(it.key())) { it = m_selectionStates.erase(it); } else { ++it; }
    }
    if (edited && !m_liveGestureWrote) {
        if (legacy) { pushLegacyControls(); } else { pushParametricToModel(); }
    } else if (edited && m_liveGestureWrote && !legacy) { pushParametricToModel(); }
    m_liveGestureWrote = false; refreshHistoryButtons();
}

void TxEqDialog::changed(bool dragging)
{
    if (m_ignoreUpdates || m_updatingFromModel) { return; }
    updateEditRowFromSelection();
    if (m_gestureActive || m_sliderActive || dragging) {
        if (m_paraLiveUpdateChk->isChecked()) {
            if (usingLegacyEq()) { pushLegacyControls(); } else { pushParametricToModel(); }
            m_liveGestureWrote = true;
        }
    } else if (!m_numericEditor) { finishEdit(); }
}

void TxEqDialog::pushLegacyControls()
{
    if (!m_radio || m_updatingFromModel) { return; }
    const QScopedValueRollback<bool> guard(m_updatingFromModel, true);
    auto& tx = m_radio->transmitModel();
    const auto profileUpdate = tx.scopedTxEqProfileUpdate();
    tx.setTxEqPreamp(m_preampSpin->value());
    for (int i = 0; i < 10; ++i) { tx.setTxEqBand(i, m_bandSpins[i]->value()); tx.setTxEqFreq(i, m_freqSpins[i]->value()); }
    tx.setTxEqNc(m_ncSpin->value()); tx.setTxEqMp(m_mpChk->isChecked());
    tx.setTxEqCtfmode(m_ctfmodeCombo->currentIndex()); tx.setTxEqWintype(m_wintypeCombo->currentIndex());
}

void TxEqDialog::rebaseEditHistory()
{
    m_gestureActive = false;
    if (m_sliderActive) {
        for (auto* slider : findChildren<QSlider*>()) { if (slider->isSliderDown()) { m_cancelledSlider = slider; QSignalBlocker b(slider); slider->setSliderDown(false); } }
    }
    m_sliderActive = false; m_numericEditor = nullptr; m_liveGestureWrote = false;
    m_selectionStates.clear();
    for (int mode = 0; mode < 2; ++mode) {
        m_history[mode]->cancelEdit(); m_committed[mode] = captureEditState(mode == 0); m_history[mode]->reset(m_committed[mode]);
    }
    cancelBandCount(); refreshHistoryButtons();
}

void TxEqDialog::refreshHistoryButtons()
{
    const int mode = usingLegacyEq() ? 0 : 1;
    m_undoBtn->setEnabled(m_history[mode]->canUndo()); m_redoBtn->setEnabled(m_history[mode]->canRedo());
}

void TxEqDialog::undoEdit()
{
    if (m_numericEditor) { m_numericEditor = nullptr; finishEdit(); }
    const bool legacy = usingLegacyEq();
    if (const auto state = m_history[legacy ? 0 : 1]->undo()) { restoreEditState(legacy, *state); }
}

void TxEqDialog::redoEdit()
{
    if (m_numericEditor) { m_numericEditor = nullptr; finishEdit(); }
    const bool legacy = usingLegacyEq();
    if (const auto state = m_history[legacy ? 0 : 1]->redo()) { restoreEditState(legacy, *state); }
}

void TxEqDialog::rebuildBandSelectors()
{
    if (!m_bandSelector) { return; }
    const auto& points = m_parametricWidget->points();
    bool rebuild = m_bandSelector->buttons().size() != points.size();
    if (!rebuild) { for (const auto& point : points) { if (!m_bandSelector->button(point.bandId)) { rebuild = true; break; } } }
    if (rebuild) {
        while (auto* item = m_bandSelectorRow->takeAt(0)) { if (auto* widget = item->widget()) { delete widget; } delete item; }
        for (const auto& point : points) {
            auto* button = new QPushButton(this); button->setCheckable(true); button->setAutoDefault(false);
            button->setMinimumWidth(86); m_bandSelector->addButton(button, point.bandId); m_bandSelectorRow->addWidget(button);
        }
    }
    const int selected = m_parametricWidget->selectedIndex();
    m_bandSelector->setExclusive(false);
    for (int i = 0; i < points.size(); ++i) {
        auto* button = m_bandSelector->button(points[i].bandId);
        button->show();
        button->setText(tr("%1\n%2 Hz").arg(points[i].bandId).arg(points[i].frequencyHz, 0, 'f', 0));
        button->setAccessibleName(tr("Band %1, %2 Hz").arg(points[i].bandId).arg(points[i].frequencyHz, 0, 'f', 0));
        QSignalBlocker b(button); button->setChecked(i == selected);
        // Layout order follows frequency while labels follow stable identity.
        m_bandSelectorRow->removeWidget(button); m_bandSelectorRow->addWidget(button);
    }
    m_bandSelector->setExclusive(true);
    // Keep one full-width row as the horizontal scroll area's content. A
    // widget-resizable host briefly compressed/overlapped the large band row
    // during count/layout changes before its minimum hint propagated.
    m_bandSelectorRow->parentWidget()->setFixedSize(m_bandSelectorRow->sizeHint());
    m_bandSelectorRow->activate();
}

void TxEqDialog::cancelBandCount()
{
    m_pendingCount = 0; m_countNotice->hide();
    QSignalBlocker b(m_bandCountGroup);
    if (auto* button = m_bandCountGroup->button(m_parametricWidget->bandCount())) { button->setChecked(true); }
}

void TxEqDialog::applyBandCount()
{
    if (m_pendingCount <= 0) { return; }
    beginEdit();
    { QSignalBlocker b(m_parametricWidget); m_parametricWidget->setBandCount(m_pendingCount); m_parametricWidget->setSelectedIndex(0); }
    cancelBandCount(); updateEditRowFromSelection(); finishEdit();
}

void TxEqDialog::changeSelectedPoint(double value, bool width)
{
    if (m_ignoreUpdates || m_updatingFromModel) { return; }
    const int index = m_parametricWidget->selectedIndex(); if (index < 0) { return; }
    if (!m_numericEditor && !m_sliderActive && !m_gestureActive) { beginEdit(); }
    const int id = m_parametricWidget->points()[index].bandId;
    ParametricEqWidget::EqJsonState state;
    state.bandCount = m_parametricWidget->bandCount(); state.parametricEq = m_parametricWidget->parametricEq();
    state.globalGainDb = m_parametricWidget->globalGainDb(); state.frequencyMinHz = m_parametricWidget->frequencyMinHz(); state.frequencyMaxHz = m_parametricWidget->frequencyMaxHz();
    state.points = m_parametricWidget->points();
    if (width) { state.points[index].q = value; } else { state.points[index].gainDb = value; }
    { QSignalBlocker b(m_parametricWidget); if (!m_parametricWidget->setEditorCurveState(state)) { return; } m_parametricWidget->setSelectedIndex(m_parametricWidget->getIndexFromBandId(id)); }
    changed();
}

bool TxEqDialog::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_cancelledSlider) {
        if (event->type() == QEvent::MouseButtonRelease) { m_cancelledSlider = nullptr; event->accept(); return true; }
        if (event->type() == QEvent::MouseMove) { event->accept(); return true; }
    }
    if (auto* line = qobject_cast<QLineEdit*>(watched); line && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        const bool undo = key->matches(QKeySequence::Undo), redo = key->matches(QKeySequence::Redo);
        if ((undo && !line->isUndoAvailable()) || (redo && !line->isRedoAvailable())) {
            if (undo) { undoEdit(); } else { redoEdit(); } key->accept(); return true;
        }
    }
    auto* spin = qobject_cast<QAbstractSpinBox*>(watched);
    if (!spin && qobject_cast<QLineEdit*>(watched)) { spin = qobject_cast<QAbstractSpinBox*>(watched->parent()); }
    if (spin && event->type() == QEvent::FocusIn && !m_ignoreUpdates && !m_updatingFromModel) {
        if (m_numericEditor != spin) {
            if (m_numericEditor) { m_numericEditor = nullptr; finishEdit(); }
            beginEdit(); m_numericEditor = spin;
        }
    }
    if (spin && event->type() == QEvent::Wheel) {
        beginEdit();
        QTimer::singleShot(0, this, [this] { m_numericEditor = nullptr; finishEdit(); });
    }
    if (qobject_cast<QSlider*>(watched) && event->type() == QEvent::MouseButtonPress) { beginEdit(); m_sliderActive = true; }
    if (qobject_cast<QSlider*>(watched) && event->type() == QEvent::MouseButtonRelease) {
        QTimer::singleShot(0, this, [this] { if (m_sliderActive) { m_sliderActive = false; finishEdit(); } });
    }
    if (qobject_cast<QSlider*>(watched) && event->type() == QEvent::Wheel) { event->accept(); return true; }
    return QDialog::eventFilter(watched, event);
}

void TxEqDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Undo) || event->matches(QKeySequence::Redo)) {
        auto* line = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        if (line && ((event->matches(QKeySequence::Undo) && line->isUndoAvailable()) || (event->matches(QKeySequence::Redo) && line->isRedoAvailable()))) {
            if (event->matches(QKeySequence::Undo)) { line->undo(); } else { line->redo(); }
        } else if (event->matches(QKeySequence::Undo)) { undoEdit(); } else { redoEdit(); }
        event->accept(); return;
    }
    QDialog::keyPressEvent(event);
}

void TxEqDialog::closeEvent(QCloseEvent* event)
{
    event->ignore();
    hide();
}

} // namespace NereusSDR
