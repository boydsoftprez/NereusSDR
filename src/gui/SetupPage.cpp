// =================================================================
// src/gui/SetupPage.cpp  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-12 - Selected-pan and page reset/copy scope for the approved rebuild.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-12 - Master display changes replace each pan override.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Shared setup-page style constants mirror AetherSDR
//                 `src/gui/RadioSetupDialog.{h,cpp}`.
//   2026-09-23 - R-R3-21: setTransmitPermitted hook and
//                 gateTransmitControls helper for transmit sections on
//                 receive pages. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-23 - R-R3-21 / R-R3-10: setStationSettingsAvailable hook and
//                 gateStationControls helper, so a page that mixes this
//                 computer's settings with the Core's disables only the
//                 Core's while the remote window is disconnected.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-09-24 - R-R3-49 (parity Task 1): setTransmitSettingsPermitted
//                 hook. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-25 - R-R3-49 (parity Task 3): setTransmitSettingsPermittedAt,
//                 the gate for settings a later transmitSettingsVersion
//                 brought. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-26: Transmit group fix wave 2 (M8): VOX shows disabled with the
//               plain reason while this computer has no microphone line to
//               the Core; the Core's refusal stays the backstop. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-10-06 - R-SPK-21: addContent() for widgets and layouts, inserted
//                 before the trailing stretch so every page starts at the
//                 top. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-10-06 - R-SPK-21 (Microphone): the gates save a control's own
//                 enabled state, not its parent's, so a control in a greyed
//                 section is not left disabled when the gate lifts.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09 - makePageTitle(): the page title, shared with RF-Kit's
//                 tabbed page. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-09 - D25: a labelled row's label keeps its 150 px column but
//                 widens when its text needs more (SetupFit::widthAtLeast), so
//                 no label is cut short. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-10 - D25: the page scrolls in a SetupFit::FitScrollArea, so a
//                 row that fits side by side without a scroll bar is side
//                 by side whatever was shown before. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-27 - Fit slider-row labels to their styled text as ordinary
//                 rows do. J.J. Boyd (KG4VCF), AI-assisted via Codex.
// =================================================================

#include "SetupPage.h"
#include <algorithm>
#include "gui/setup/SetupFit.h"
#include "StyleConstants.h"
#include "SpectrumWidget.h"
#include "ColorSwatchButton.h"
#include "gui/setup/SetupHelpers.h"
#include "gui/widgets/GradientPickerWidget.h"
#include "gui/styles/WorkspacePreview.h"
#include "core/AppSettings.h"
#include "core/settings/PanDisplayKeys.h"
#include "models/DisplaySettingsModel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/PanadapterModel.h"

#include <QCheckBox>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QVariant>
#include <QFrame>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QSlider>
#include <QSpinBox>
#include <QDialog>
#include <QScreen>

namespace NereusSDR {

// Shared style strings — mirror AetherSDR RadioSetupDialog constants
static const QString kGroupStyle =
    "QGroupBox { border: 1px solid #304050; border-radius: 4px; "
    "margin-top: 8px; padding-top: 12px; font-weight: bold; color: #8aa8c0; }"
    "QGroupBox::title { subcontrol-origin: margin; left: 10px; "
    "padding: 0 4px; }";

static const QString kLabelStyle =
    "QLabel { color: #c8d8e8; font-size: 12px; }";

// ── Construction ──────────────────────────────────────────────────────────────

SetupPage::SetupPage(const QString& title, RadioModel* model, QWidget* parent)
    : QWidget(parent), m_title(title), m_model(model)
{
    init(title);
}

SetupPage::SetupPage(const QString& title, QWidget* parent)
    : QWidget(parent), m_title(title), m_model(nullptr)
{
    init(title);
}

void SetupPage::init(const QString& title)
{
    // Apply dark-theme checkbox + radio-button styles at the page root
    // so every QCheckBox / QRadioButton in any derived SetupPage
    // inherits visible indicators over the #0f0f1a background.
    // Without this, system-default indicators render black-on-dark
    // and are invisible to users. Fixes regression reported
    // 2026-04-21 on Phase 3P-H pre-PR debug.
    setStyleSheet(QString("%1 %2").arg(Style::kCheckBoxStyle,
                                       Style::kRadioButtonStyle));

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 8, 12, 8);
    rootLayout->setSpacing(6);

    // Page title
    rootLayout->addWidget(makePageTitle(title));

    // Scrollable content area. D25: rows that fit side by side without a
    // vertical scroll bar stay side by side (SetupFit::FitScrollArea).
    auto* scroll = new SetupFit::FitScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("QScrollArea { background: #0f0f1a; border: none; }");

    auto* contentWidget = new QWidget;
    contentWidget->setStyleSheet("QWidget { background: #0f0f1a; }");

    m_contentLayout = new QVBoxLayout(contentWidget);
    m_contentLayout->setContentsMargins(0, 4, 0, 4);
    m_contentLayout->setSpacing(6);
    m_contentLayout->addStretch(1);

    scroll->setWidget(contentWidget);
    rootLayout->addWidget(scroll, 1);
}

QLabel* SetupPage::makePageTitle(const QString& title, QWidget* parent)
{
    auto* titleLabel = new QLabel(title, parent);
    titleLabel->setStyleSheet(
        "QLabel { color: #c8d8e8; font-size: 16px; font-weight: bold; "
        "border-bottom: 1px solid #304050; padding-bottom: 4px; }");
    return titleLabel;
}

// ── Virtual ───────────────────────────────────────────────────────────────────

void SetupPage::syncFromModel()
{
    // Base implementation is a no-op; subclasses override to pull from RadioModel.
}

// ── Section helper ────────────────────────────────────────────────────────────

void SetupPage::setTransmitPermitted(bool /*permitted*/, const QString& /*reason*/)
{
}

void SetupPage::setVoxPermitted(bool /*permitted*/, const QString& /*reason*/)
{
}

void SetupPage::setTransmitSettingsPermitted(bool /*permitted*/, const QString& /*reason*/)
{
}

void SetupPage::setTransmitSettingsPermittedAt(int /*version*/, bool /*permitted*/,
                                               const QString& /*reason*/)
{
}

void SetupPage::setStationSettingsAvailable(bool /*available*/, const QString& /*reason*/)
{
}

void SetupPage::setFftHandlers(const SetupFftHandlers& handlers)
{
    m_fftHandlers = handlers;
}

void SetupPage::stationFftSettingsChanged()
{
}

// ── Display defaults (Display pill plan Task 21) ─────────────────────────────

namespace {

// Every live page that follows a display default. A default changed on one
// page is shown again on the others (two Setup pages can show the same key,
// and a pan with a value of its own announces nothing).
QList<SetupPage*>& displayFollowerPages()
{
    static QList<SetupPage*> pages;
    return pages;
}

QString defaultBoolText(bool on)
{
    return on ? QStringLiteral("True") : QStringLiteral("False");
}

const QStringList& waterfallModeKeys()
{
    static const QStringList keys{QStringLiteral("ClarityEnabled"), QStringLiteral("DisplayWfAgc"),
        QStringLiteral("WaterfallNFAGCEnabled"), QStringLiteral("DisplayWfUseSpectrumMinMax")};
    return keys;
}

bool isSharedDisplaySetting(const QString& key)
{
    static const QSet<QString> keys{QStringLiteral("DisplayGridColor"), QStringLiteral("DisplayGridFineColor"),
        QStringLiteral("DisplayHGridColor"), QStringLiteral("DisplayGridTextColor"), QStringLiteral("DisplayBandEdgeColor"),
        QStringLiteral("DisplayRxZeroLineColor"), QStringLiteral("DisplayTxZeroLineColor"),
        QStringLiteral("DisplayRxFilterColor"), QStringLiteral("DisplayTxFilterColor"),
        QStringLiteral("DisplayTxRef"), QStringLiteral("DisplayTxRange"),
        QStringLiteral("DisplayTxWfPalette"), QStringLiteral("DisplayTxWfLowLevel"),
        QStringLiteral("DisplayTxWfHighLevel"), QStringLiteral("DisplayTxWfLowColor"),
        QStringLiteral("DisplayTxWfGradient"), QStringLiteral("DisplayWfCustomStops")};
    return keys.contains(key);
}

} // namespace

SetupPage::~SetupPage()
{
    displayFollowerPages().removeAll(this);
}

void SetupPage::setDisplayProviders(const SetupDisplayProviders& providers)
{
    m_displayProviders = providers;
    refreshDisplayPanScope();
    refreshDisplayBandPicker();
    if (m_displayFollowers.isEmpty()) { return; }
    followPanWidgets();
    showDisplayDefaults();
    displayPansChanged();
    refreshDisplayBandPicker();
}

QList<SpectrumWidget*> SetupPage::panWidgets() const
{
    QList<SpectrumWidget*> pans;
    if (m_displayProviders.panWidgets) {
        for (SpectrumWidget* pan : m_displayProviders.panWidgets()) {
            if (pan && !pans.contains(pan)) { pans.append(pan); }
        }
    } else if (m_fallbackPan) {
        pans.append(static_cast<SpectrumWidget*>(m_fallbackPan.data()));
    } else if (m_model && m_model->spectrumWidget()) {
        pans.append(m_model->spectrumWidget());
    }
    return pans;
}

PanadapterModel* SetupPage::bandGridModel() const
{
    if (m_displayPanIndex >= 0 && m_displayProviders.bandGridModelForPan) {
        return m_displayProviders.bandGridModelForPan(m_displayPanIndex);
    }
    return m_displayProviders.bandGridModel ? m_displayProviders.bandGridModel() : nullptr;
}

SpectrumWidget* SetupPage::selectedDisplayPan() const
{
    if (m_displayPanIndex < 0) { return nullptr; }
    for (SpectrumWidget* pan : panWidgets()) {
        if (pan->panIndex() == m_displayPanIndex) { return pan; }
    }
    return nullptr;
}

void SetupPage::setDisplayPanIndex(int panIndex)
{
    const int selected = std::max(-1, panIndex);
    if (selected == m_displayPanIndex) { return; }
    m_displayPanIndex = selected;
    showDisplayDefaults();
    displayPansChanged();
    emit displayPanIndexChanged(selected);
    refreshDisplayPanScope();
    refreshDisplayBandPicker();
}

void SetupPage::enableDisplayBandPicker()
{
    if (m_displayBandPicker) { return; }
    auto* row = new QWidget(this);
    row->setObjectName(QStringLiteral("setupDisplayBandRow"));
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel(QStringLiteral("Band:"), row));
    m_displayBandPicker = new QComboBox(row);
    m_displayBandPicker->setObjectName(QStringLiteral("setupDisplayBandPicker"));
    for (int index = 0; index < static_cast<int>(Band::Count); ++index) {
        const Band band = static_cast<Band>(index);
        m_displayBandPicker->addItem(bandLabel(band), index);
    }
    layout->addWidget(m_displayBandPicker);
    layout->addStretch(1);
    connect(m_displayBandPicker, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (selectedDisplayPan()) { return; }
        if (PanadapterModel* grid = bandGridModel()) {
            grid->setBand(static_cast<Band>(m_displayBandPicker->itemData(index).toInt()));
            displayPansChanged();
        }
    });
    m_contentLayout->insertWidget(m_displayScopeStrip ? 1 : 0, row);
    refreshDisplayBandPicker();
}

void SetupPage::refreshDisplayBandPicker()
{
    if (!m_displayBandPicker) { return; }
    disconnect(m_displayBandConnection);
    PanadapterModel* grid = bandGridModel();
    const bool open = selectedDisplayPan() != nullptr;
    if (grid) {
        const QSignalBlocker blocker(m_displayBandPicker);
        m_displayBandPicker->setCurrentIndex(m_displayBandPicker->findData(static_cast<int>(grid->band())));
        m_displayBandConnection = connect(grid, &PanadapterModel::bandChanged, this, [this](Band) {
            refreshDisplayBandPicker();
            displayPansChanged();
        });
    }
    m_displayBandPicker->setEnabled(grid && !open);
    m_displayBandPicker->setToolTip(open
        ? QStringLiteral("This open pan's band sets these rows.")
        : QStringLiteral("Choose the band whose saved dB Max, dB Min and 3D Floor you want to edit."));
}

void SetupPage::setDisplayPageKeyBases(const QStringList& keys)
{
    m_displayScopeKeys = keys;
    m_displayScopeKeys.removeDuplicates();
    m_hasDisplayScopeKeys = true;
    refreshDisplayPanScope();
}

void SetupPage::enableDisplayPanScope()
{
    if (m_displayScopeStrip) { return; }
    m_displayScopeStrip = new QWidget(this);
    m_displayScopeStrip->setObjectName(QStringLiteral("setupDisplayPanScope"));
    auto* stripLayout = new QVBoxLayout(m_displayScopeStrip);
    stripLayout->setContentsMargins(0, 0, 0, 0);
    m_displayScopeChipLayout = new SetupFit::FitRow;
    m_displayScopeChipLayout->setContentsMargins(0, 0, 0, 0);
    m_displayScopeChipLayout->setSpacing(6);
    auto* label = new QLabel(QStringLiteral("Settings for:"), m_displayScopeStrip);
    m_displayScopeChipLayout->addWidget(label);
    stripLayout->addLayout(m_displayScopeChipLayout);
    m_displayScopeNote = new QLabel(m_displayScopeStrip);
    m_displayScopeNote->setObjectName(QStringLiteral("setupDisplayPanNote"));
    m_displayScopeNote->setWordWrap(true);
    stripLayout->addWidget(m_displayScopeNote);
    auto* actions = new SetupFit::FitRow;
    m_displayScopeReset = new QPushButton(QStringLiteral("Reset this pan"), m_displayScopeStrip);
    m_displayScopeReset->setObjectName(QStringLiteral("setupDisplayPageReset"));
    m_displayScopeCopy = new QPushButton(m_displayScopeStrip);
    m_displayScopeCopy->setObjectName(QStringLiteral("setupDisplayPageCopy"));
    actions->addWidget(m_displayScopeReset);
    actions->addWidget(m_displayScopeCopy);
    actions->addStretch(1);
    stripLayout->addLayout(actions);
    connect(m_displayScopeReset, &QPushButton::clicked, this, &SetupPage::resetDisplayPageOverrides);
    connect(m_displayScopeCopy, &QPushButton::clicked, this, &SetupPage::copyDisplayPageToAll);
    m_contentLayout->insertWidget(0, m_displayScopeStrip);
    refreshDisplayPanScope();
}

void SetupPage::refreshDisplayPanScope()
{
    if (!m_displayScopeStrip) { return; }
    QList<int> indices = m_displayProviders.panIndices ? m_displayProviders.panIndices() : QList<int>();
    const QList<SpectrumWidget*> pans = panWidgets();
    for (SpectrumWidget* pan : pans) { indices.append(pan->panIndex()); }
    if (m_displayPanIndex >= 0) { indices.append(m_displayPanIndex); }
    indices.removeAll(-1);
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    indices.prepend(-1);
    for (int position = 0; position < indices.size(); ++position) {
        const int index = indices.at(position);
        QPushButton* chip = m_displayScopeChips.value(index);
        if (!chip) {
            chip = new QPushButton(m_displayScopeStrip);
            chip->setCheckable(true);
            chip->setStyleSheet(Style::buttonBaseStyle() + Style::blueCheckedStyle());
            chip->setObjectName(index < 0 ? QStringLiteral("setupDisplayPanAll")
                                         : QStringLiteral("setupDisplayPan%1").arg(index));
            connect(chip, &QPushButton::clicked, this, [this, index]() { setDisplayPanIndex(index); });
            m_displayScopeChips.insert(index, chip);
            m_displayScopeChipLayout->insertWidget(position + 1, chip);
        }
        bool open = false;
        for (SpectrumWidget* pan : pans) { if (pan->panIndex() == index) { open = true; break; } }
        QStringList letters;
        if (m_model && open) {
            for (SliceModel* slice : m_model->slices()) {
                if (slice->panKey() == QStringLiteral("pan-%1").arg(index)) {
                    letters.append(QString(slice->sliceLetter()));
                }
            }
        }
        const int ownCount = displayPageOwnCount(index);
        chip->setProperty("ownValueCount", ownCount);
        const QString subtitle = open ? (letters.isEmpty() ? QStringLiteral("open")
            : QStringLiteral("Slice %1").arg(letters.join(QStringLiteral(", ")))) : QStringLiteral("not open");
        chip->setText(index < 0 ? QStringLiteral("All pans")
            : QStringLiteral("Pan %1%2\n%3").arg(index + 1).arg(ownCount ? QStringLiteral(" •") : QString()).arg(subtitle));
        const QSignalBlocker blocker(chip);
        chip->setChecked(index == m_displayPanIndex);
        chip->show();
    }
    for (auto chip = m_displayScopeChips.cbegin(); chip != m_displayScopeChips.cend(); ++chip) {
        if (!indices.contains(chip.key())) { chip.value()->hide(); }
    }
    const bool selected = m_displayPanIndex >= 0;
    const int ownCount = displayPageOwnCount(m_displayPanIndex);
    const QString follows = QStringLiteral("Pan %1 follows All pans in every row on this page.").arg(m_displayPanIndex + 1);
    QString note;
    if (!selected) {
        note = QStringLiteral("A change here goes to every pan.");
    } else {
        if (!selectedDisplayPan()) {
            note = QStringLiteral("Pan %1 is not open now. What you set here is kept and used when you open it. ")
                       .arg(m_displayPanIndex + 1);
        }
        note += ownCount ? QStringLiteral("Pan %1 has its own value in %2 row%3 on this page, marked with a dot.")
                            .arg(m_displayPanIndex + 1).arg(ownCount).arg(ownCount == 1 ? QString() : QStringLiteral("s"))
                         : follows;
    }
    m_displayScopeNote->setText(note);
    m_displayScopeReset->setVisible(selected);
    m_displayScopeCopy->setVisible(selected);
    m_displayScopeReset->setEnabled(selected && ownCount > 0);
    m_displayScopeCopy->setEnabled(selected && ownCount > 0);
    m_displayScopeReset->setToolTip(ownCount ? QStringLiteral("Reset this pan's own values on this page to the Setup defaults.") : follows);
    m_displayScopeCopy->setText(QStringLiteral("Use Pan %1's on all pans").arg(m_displayPanIndex + 1));
    m_displayScopeCopy->setToolTip(ownCount ? QStringLiteral("Use this pan's own values on this page as the defaults for every pan.") : follows);
}

QStringList SetupPage::displayPageKeyBases() const
{
    QStringList keys = m_hasDisplayScopeKeys ? m_displayScopeKeys : m_displayFollowers.keys();
    keys.sort();
    return keys;
}

int SetupPage::displayPageOwnCount(int panIndex) const
{
    if (panIndex < 0) { return 0; }
    SpectrumWidget* open = nullptr;
    for (SpectrumWidget* pan : panWidgets()) {
        if (pan->panIndex() == panIndex) { open = pan; break; }
    }
    int count = 0;
    bool waterfallModeOwn = false;
    for (const QString& key : displayPageKeyBases()) {
        bool own = false;
        if (open) {
            QString master = AppSettings::instance().value(PanDisplayKeys::defaultKey(key)).toString();
            if (master.isEmpty()) { master = open->perPanShipDefault(key); }
            own = !PanDisplayKeys::valuesEqual(open->perPanDisplayValue(key), master);
        } else if (PanDisplayKeys::hasOverride(key, panIndex)) {
            own = true;
        }
        if (waterfallModeKeys().contains(key)) { waterfallModeOwn = waterfallModeOwn || own; }
        else if (own) { ++count; }
    }
    return count + (waterfallModeOwn ? 1 : 0);
}

void SetupPage::resetDisplayPageOverrides()
{
    if (m_displayPanIndex < 0) { return; }
    const QStringList keys = displayPageKeyBases();
    if (SpectrumWidget* pan = selectedDisplayPan()) {
        pan->resetPanDisplayOverridesForKeys(keys);
    } else {
        PanDisplayKeys::clearPan(m_displayPanIndex, keys);
    }
    showDisplayDefaults();
    displayPansChanged();
    refreshDisplayPanScope();
}

void SetupPage::convertManualWaterfallModesForSmoothReset()
{
    const QPointer<SetupPage> alive(this);
    const int selectedIndex = m_displayPanIndex;
    const QString agcKey = QStringLiteral("DisplayWfAgc");
    const QString nfKey = QStringLiteral("WaterfallNFAGCEnabled");
    const QStringList keys = waterfallModeKeys();
    const QList<SpectrumWidget*> pans = panWidgets();
    if (!alive) { return; }
    QHash<int, QPointer<SpectrumWidget>> open;
    QSet<int> indices;
    for (SpectrumWidget* pan : pans) {
        open.insert(pan->panIndex(), pan);
        indices.insert(pan->panIndex());
    }
    // Capture weak recipients before a settings callback can retire another.
    for (const QPointer<SpectrumWidget>& pan : std::as_const(open)) {
        for (const QString& key : keys) {
            if (pan) { pan->storePendingPerPanSetting(key); }
            if (!alive || m_displayPanIndex != selectedIndex) { return; }
        }
    }
    const std::function<QList<int>()> rememberedIndices = m_displayProviders.panIndices;
    if (rememberedIndices) {
        const QList<int> remembered = rememberedIndices();
        if (!alive || m_displayPanIndex != selectedIndex) { return; }
        for (int index : remembered) {
            if (index >= 0) { indices.insert(index); }
        }
    }
    // Include remembered closed pans without treating unrelated suffixes
    // (or noncanonical numbers) as pan indices.
    for (const QString& storedKey : AppSettings::instance().allKeys()) {
        for (const QString& key : keys) {
            if (!storedKey.startsWith(key + QLatin1Char('_'))) { continue; }
            bool ok = false;
            const int index = storedKey.mid(key.size() + 1).toInt(&ok);
            if (ok && index >= 0 && storedKey == PanDisplayKeys::panKey(key, index)) {
                indices.insert(index);
            }
        }
    }
    const auto read = [&open](const QString& key, int index) {
        if (index >= 0 && open.value(index)) { return open.value(index)->perPanDisplayValue(key); }
        const QString fallback = SpectrumWidget::perPanShipDefaultForClosedPan(key);
        return index < 0 ? AppSettings::instance().value(key, fallback).toString()
                         : PanDisplayKeys::read(key, index, fallback).toString();
    };
    const auto isManual = [&agcKey, &read](int index) {
        // Same precedence as the native chooser: NF does not participate
        // while AGC is off, even when its remembered bit is True.
        return read(QStringLiteral("ClarityEnabled"), index) != QStringLiteral("True")
            && read(QStringLiteral("DisplayWfUseSpectrumMinMax"), index) != QStringLiteral("True")
            && read(agcKey, index) != QStringLiteral("True");
    };
    const auto refreshPages = [&alive]() {
        QList<QPointer<SetupPage>> pages;
        for (SetupPage* page : displayFollowerPages()) { pages.append(page); }
        for (const QPointer<SetupPage>& page : pages) {
            if (page) { page->showDisplayDefaults(); }
            if (!alive) { return; }
        }
    };
    if (selectedIndex >= 0) {
        if (!isManual(selectedIndex)) { return; }
        if (open.value(selectedIndex)) {
            open.value(selectedIndex)->enableWaterfallAgcIfManual();
            if (!alive || m_displayPanIndex != selectedIndex) { return; }
        } else {
            PanDisplayKeys::writeSparse(nfKey, selectedIndex, QStringLiteral("False"),
                SpectrumWidget::perPanShipDefaultForClosedPan(nfKey));
            if (!alive || m_displayPanIndex != selectedIndex) { return; }
            PanDisplayKeys::writeSparse(agcKey, selectedIndex, QStringLiteral("True"),
                SpectrumWidget::perPanShipDefaultForClosedPan(agcKey));
            if (!alive || m_displayPanIndex != selectedIndex) { return; }
        }
        refreshPages();
        return;
    }
    QHash<int, QString> desiredAgc;
    QHash<int, QString> desiredNf;
    QSet<int> manualPans;
    for (int index : std::as_const(indices)) {
        const bool manual = isManual(index);
        desiredAgc.insert(index, manual ? QStringLiteral("True") : read(agcKey, index));
        desiredNf.insert(index, manual ? QStringLiteral("False") : read(nfKey, index));
        if (manual) { manualPans.insert(index); }
    }
    // A Manual master becomes Auto. Preserve each non-Manual pan's
    // remembered inactive AGC flag when the master write clears overrides.
    // Snapshot and restore NF too: clearing the master NF bit must not
    // turn a remembered closed Floor tuple into Auto or lose inactive bits.
    if (isManual(-1)) {
        PanDisplayKeys::writeDefault(nfKey, QStringLiteral("False"));
        if (!alive || m_displayPanIndex != selectedIndex) { return; }
        PanDisplayKeys::writeDefault(agcKey, QStringLiteral("True"));
        if (!alive || m_displayPanIndex != selectedIndex) { return; }
    }
    for (auto desired = desiredAgc.cbegin(); desired != desiredAgc.cend(); ++desired) {
        PanDisplayKeys::writeSparse(nfKey, desired.key(), desiredNf.value(desired.key()),
            SpectrumWidget::perPanShipDefaultForClosedPan(nfKey));
        if (!alive || m_displayPanIndex != selectedIndex) { return; }
        PanDisplayKeys::writeSparse(agcKey, desired.key(), desired.value(),
            SpectrumWidget::perPanShipDefaultForClosedPan(agcKey));
        if (!alive || m_displayPanIndex != selectedIndex) { return; }
    }
    for (auto pan = open.cbegin(); pan != open.cend(); ++pan) {
        if (pan.value() && manualPans.contains(pan.key())) {
            pan.value()->enableWaterfallAgcIfManual();
        }
        if (!alive || m_displayPanIndex != selectedIndex) { return; }
    }
    refreshPages();
}

void SetupPage::addDisplayBackgroundSection()
{
    if (findChild<QGroupBox*>(QStringLiteral("setupDisplayBackground"))) { return; }
    QGroupBox* group = addSection(QStringLiteral("Background"));
    group->setObjectName(QStringLiteral("setupDisplayBackground"));
    group->setProperty("displayAssemblyGroupTitle", QStringLiteral("Background"));
    auto* form = new QFormLayout;
    qobject_cast<QVBoxLayout*>(group->layout())->addLayout(form);
    auto* enabled = new QCheckBox(QStringLiteral("Background"), group);
    enabled->setProperty("nereusSetupId", QStringLiteral("display.view.backgroundEnabled"));
    form->addRow(enabled);
    bindDefaultToggle(enabled, QStringLiteral("DisplayBackgroundEnabled"),
        [](SpectrumWidget* pan, bool on) { pan->setBackgroundEnabled(on); });
    auto* colour = new ColorSwatchButton(QColor(QStringLiteral("#0a0a14")), group);
    colour->setProperty("nereusSetupId", QStringLiteral("display.view.backgroundColor"));
    form->addRow(QStringLiteral("Color"), colour);
    connect(colour, &ColorSwatchButton::colorChanged, this, [this](QColor value) {
        value.setAlpha(255);
        applyDisplayDefault(QStringLiteral("DisplayBackgroundColor"), value.name(QColor::HexArgb),
            [value](SpectrumWidget* pan) { pan->setBackgroundFillColor(value); });
    });
    followDisplayDefault(QStringLiteral("DisplayBackgroundColor"), [this, colour]() {
        QColor value(displayDefaultText(QStringLiteral("DisplayBackgroundColor")));
        if (!value.isValid()) { value = QColor(QStringLiteral("#0a0a14")); }
        value.setAlpha(255);
        const QSignalBlocker blocker(colour);
        colour->setColor(value);
    });
    auto* pictureRow = new QWidget(group);
    auto* pictureLine = new QHBoxLayout(pictureRow);
    pictureLine->setContentsMargins(0, 0, 0, 0);
    auto* choose = new QPushButton(QStringLiteral("Choose picture…"), pictureRow);
    choose->setProperty("nereusSetupId", QStringLiteral("display.view.backgroundImage"));
    auto* clear = new QPushButton(QStringLiteral("Clear"), pictureRow);
    clear->setProperty("nereusSetupId", QStringLiteral("display.view.clearBackgroundImage"));
    auto* file = new QLabel(pictureRow);
    file->setWordWrap(true);
    pictureLine->addWidget(choose);
    pictureLine->addWidget(clear);
    pictureLine->addWidget(file, 1);
    form->addRow(QStringLiteral("Picture"), pictureRow);
    const auto setImage = [this](const QString& path) {
        applyDisplayDefault(QStringLiteral("DisplayBackgroundImage"), path,
            [path](SpectrumWidget* pan) { pan->setBackgroundImagePath(path); });
    };
    connect(clear, &QPushButton::clicked, this, [setImage]() { setImage(QString()); });
    connect(choose, &QPushButton::clicked, this, [this, setImage]() {
        const int scope = m_displayPanIndex;
        auto* picker = new QFileDialog(this, QStringLiteral("Choose background picture"));
        picker->setAttribute(Qt::WA_DeleteOnClose);
        picker->setFileMode(QFileDialog::ExistingFile);
        picker->setNameFilter(QStringLiteral("Pictures (*.png *.jpg *.jpeg *.bmp *.webp)"));
        connect(picker, &QFileDialog::fileSelected, this, [this, scope, setImage](const QString& path) {
            if (scope != m_displayPanIndex) { return; }
            const QPointer<SetupPage> alive(this);
            setImage(path);
            if (!alive) { return; }
            applyDisplayDefault(QStringLiteral("DisplayBackgroundEnabled"), QStringLiteral("True"),
                [](SpectrumWidget* pan) { pan->setBackgroundEnabled(true); });
        });
        picker->open();
    });
    SliderRow opacity = makeSliderRow(0, 100, 80, QStringLiteral("%"), group);
    opacity.slider->setProperty("nereusSetupId", QStringLiteral("display.view.backgroundOpacity"));
    form->addRow(QStringLiteral("Opacity"), opacity.container);
    bindDefaultSlider(opacity.slider, opacity.spin, QStringLiteral("DisplayBackgroundOpacity"),
        [](SpectrumWidget* pan, int value) { pan->setBackgroundOpacity(value); });
    auto* reason = new QLabel(group);
    reason->setWordWrap(true);
    group->layout()->addWidget(reason);
    const auto refresh = [this, enabled, colour, choose, clear, file, opacity, reason]() {
        const bool threeD = displayDefaultText(QStringLiteral("DisplaySpectrumRenderMode")).toInt()
            == static_cast<int>(SpectrumRenderMode::Mode3D);
        const bool on = displayDefaultText(QStringLiteral("DisplayBackgroundEnabled")) == QStringLiteral("True");
        const QString path = displayDefaultText(QStringLiteral("DisplayBackgroundImage"));
        const QString unavailable = QStringLiteral("The 3D view does not draw the background. Pick 2D Waterfall above to use it.");
        enabled->setEnabled(!threeD);
        choose->setEnabled(!threeD);
        clear->setEnabled(!threeD && !path.isEmpty());
        colour->setEnabled(!threeD && on);
        opacity.container->setEnabled(!threeD && on && !path.isEmpty());
        enabled->setToolTip(threeD ? unavailable : QString());
        choose->setToolTip(threeD ? unavailable : QStringLiteral("Choose a picture and turn Background on."));
        colour->setToolTip(threeD ? unavailable : on ? QString() : QStringLiteral("Turn Background on to use its color."));
        opacity.container->setToolTip(threeD ? unavailable : !on ? QStringLiteral("Turn Background on to use the picture.")
            : path.isEmpty() ? QStringLiteral("Choose a picture to use Opacity.") : QString());
        file->setText(path.isEmpty() ? QStringLiteral("No picture chosen") : QFileInfo(path).fileName());
        file->setToolTip(path);
        reason->setText(threeD ? unavailable : QString());
        reason->setVisible(threeD);
    };
    for (const QString& key : {QStringLiteral("DisplayBackgroundEnabled"), QStringLiteral("DisplayBackgroundImage"),
                               QStringLiteral("DisplaySpectrumRenderMode")}) {
        followDisplayDefault(key, refresh);
    }
    followPanWidgets();
    showDisplayDefaults();
}

void SetupPage::addDisplayGradientEditor(bool transmit)
{
    const QString id = transmit ? QStringLiteral("display.txDisplay.customGradient")
                                : QStringLiteral("display.waterfall.customGradient");
    if (findChild<QWidget*>(id)) { return; }
    auto* row = new QWidget(this);
    row->setObjectName(id);
    row->setProperty("nereusSetupId", id);
    auto* line = new QVBoxLayout(row);
    line->setContentsMargins(0, 0, 0, 0);
    auto* edit = new QPushButton(QStringLiteral("Edit Custom gradient…"), row);
    edit->setProperty("nereusSetupId", transmit ? QStringLiteral("display.txDisplay.editCustomGradient")
                                               : QStringLiteral("display.waterfall.editCustomGradient"));
    auto* reason = new QLabel(row);
    reason->setWordWrap(true);
    line->addWidget(edit);
    line->addWidget(reason);
    addContent(row);
    const QString paletteKey = transmit ? QStringLiteral("DisplayTxWfPalette") : QStringLiteral("DisplayWfColorScheme");
    const QString definitionKey = transmit ? QStringLiteral("DisplayTxWfGradient") : QStringLiteral("DisplayWfCustomStops");
    followDisplayDefault(paletteKey, [this, edit, reason, paletteKey]() {
        const bool custom = displayDefaultText(paletteKey).toInt() == static_cast<int>(WfColorScheme::Custom);
        const QString why = QStringLiteral("Pick Custom above to edit its gradient.");
        edit->setEnabled(custom);
        edit->setToolTip(custom ? QString() : why);
        reason->setText(custom ? QString() : why);
        reason->setVisible(!custom);
    });
    connect(edit, &QPushButton::clicked, this, [this, edit, transmit, definitionKey]() {
        auto* popup = new QDialog(this, Qt::Tool);
        popup->setAttribute(Qt::WA_DeleteOnClose);
        popup->setWindowTitle(transmit ? QStringLiteral("TX Custom gradient") : QStringLiteral("RX Custom gradient"));
        auto* column = new QVBoxLayout(popup);
        auto* picker = new GradientPickerWidget(popup);
        QString encoded = AppSettings::instance().value(definitionKey).toString();
        if (!transmit && encoded.isEmpty()) { encoded = SpectrumWidget::defaultRxWfGradient(); }
        picker->setEncodedText(encoded);
        column->addWidget(picker);
        auto* instructions = new QLabel(QStringLiteral("Click to add a stop, drag to move it, double-click to change its color, or right-click to remove it. The two end stops stay in place."), popup);
        instructions->setWordWrap(true);
        column->addWidget(instructions);
        auto* close = new QPushButton(QStringLiteral("Close"), popup);
        column->addWidget(close);
        connect(close, &QPushButton::clicked, popup, &QDialog::accept);
        connect(picker, &GradientPickerWidget::changed, this, [this, picker, transmit, definitionKey]() {
            const QString value = picker->encodedText();
            const QList<SpectrumWidget*> pans = panWidgets();
            // Custom definitions belong to this computer. The widget setter
            // updates all live recipients; no-open editing writes the same
            // shared definition without constructing a renderer.
            if (pans.isEmpty()) { PanDisplayKeys::writeDefault(definitionKey, value); }
            else if (transmit) { pans.first()->setTxWfGradient(value); }
            else { pans.first()->setRxWfGradient(value); }
        });
        WorkspacePreview::themeOnShow(popup);
        popup->resize(420, popup->sizeHint().height());
        popup->show();
        // The editor follows its button and opens above when the screen has
        // insufficient room below, including Setup near the bottom edge.
        if (QScreen* screen = edit->screen()) {
            const QRect available = screen->availableGeometry();
            const QPoint below = edit->mapToGlobal(QPoint(0, edit->height()));
            const int above = edit->mapToGlobal(QPoint(0, 0)).y() - popup->height();
            const int y = below.y() + popup->height() <= available.bottom() + 1 ? below.y() : above;
            popup->move(qBound(available.left(), below.x(), std::max(available.left(), available.right() + 1 - popup->width())),
                        qBound(available.top(), y, std::max(available.top(), available.bottom() + 1 - popup->height())));
        }
    });
    followPanWidgets();
    showDisplayDefaults();
}

void SetupPage::copyDisplayPageToAll()
{
    if (m_displayPanIndex < 0) { return; }
    const QList<SpectrumWidget*> pans = panWidgets();
    const QStringList keys = displayPageKeyBases();
    for (SpectrumWidget* pan : pans) {
        for (const QString& key : keys) { pan->storePendingPerPanSetting(key); }
    }
    QHash<QString, QVariant> pending;
    for (const QString& key : keys) {
        if (PanDisplayKeys::hasOverride(key, m_displayPanIndex)) {
            pending.insert(key, AppSettings::instance().value(PanDisplayKeys::panKey(key, m_displayPanIndex)));
        }
    }
    // A mode is one row. Copy its complete remembered tuple when that row
    // differs, so every pan receives the same requested mode and flags.
    bool copyWaterfallMode = false;
    for (const QString& key : waterfallModeKeys()) {
        copyWaterfallMode = copyWaterfallMode || pending.contains(key);
    }
    if (copyWaterfallMode) {
        for (const QString& key : waterfallModeKeys()) {
            pending.insert(key, displayDefaultText(key));
        }
    }
    // Accepted call 42: copying just one end stops at the master's other
    // end. Both copied ends use the same legal range as a manual edit.
    constexpr float kWaterfallPairMinDbm = -200.0f;
    constexpr float kWaterfallPairMaxDbm = 0.0f;
    constexpr float kWaterfallPairGapDb = 5.0f;
    const QString lowKey = QStringLiteral("DisplayWfLowLevel");
    const QString highKey = QStringLiteral("DisplayWfHighLevel");
    if (pending.contains(lowKey) || pending.contains(highKey)) {
        const auto valueFor = [&pending](const QString& key) {
            if (pending.contains(key)) { return pending.value(key).toFloat(); }
            QString text = AppSettings::instance().value(PanDisplayKeys::defaultKey(key)).toString();
            if (text.isEmpty()) { text = SpectrumWidget::perPanShipDefaultForClosedPan(key); }
            return text.toFloat();
        };
        float low = qBound(kWaterfallPairMinDbm, valueFor(lowKey), kWaterfallPairMaxDbm - kWaterfallPairGapDb);
        float high = qBound(kWaterfallPairMinDbm + kWaterfallPairGapDb, valueFor(highKey), kWaterfallPairMaxDbm);
        if (pending.contains(lowKey)) {
            low = std::min(low, high - kWaterfallPairGapDb);
            pending[lowKey] = QString::number(low);
        }
        if (pending.contains(highKey)) {
            high = std::max(high, low + kWaterfallPairGapDb);
            pending[highKey] = QString::number(high);
        }
    }
    QStringList copied = pending.keys();
    copied.sort();
    for (const QString& key : copied) { PanDisplayKeys::writeDefault(key, pending.value(key)); }
    if (copied.isEmpty()) { return; }
    for (SpectrumWidget* pan : pans) { pan->resetPanDisplayOverridesForKeys(copied); }
    if (m_model && copied.contains(QStringLiteral("ClarityEnabled"))) {
        m_model->setClarityEnabled(AppSettings::instance().value(QStringLiteral("ClarityEnabled")).toString()
                                  == QStringLiteral("True"));
    }
    for (SetupPage* page : displayFollowerPages()) { page->showDisplayDefaults(); }
    displayPansChanged();
}

void SetupPage::applyDisplayDefault(const QString& keyBase, const QVariant& value,
                                    const std::function<void(SpectrumWidget*)>& apply)
{
    if (m_displayPanIndex >= 0 && !isSharedDisplaySetting(keyBase)) {
        if (PanDisplayKeys::valuesEqual(displayDefaultText(keyBase), value)) { return; }
        if (SpectrumWidget* pan = selectedDisplayPan()) {
            if (apply) { apply(pan); }
        } else {
            const QList<SpectrumWidget*> pans = panWidgets();
            const QString fallback = pans.isEmpty() ? SpectrumWidget::perPanShipDefaultForClosedPan(keyBase)
                                                     : pans.first()->perPanShipDefault(keyBase);
            PanDisplayKeys::writeSparse(keyBase, m_displayPanIndex, value, fallback);
        }
        for (SetupPage* page : displayFollowerPages()) { page->showDisplayDefault(keyBase); }
        return;
    }
    // Flush a pending change against the old master before replacing it.
    // The approved master rule then clears this setting's overrides, so
    // every pan takes the new value, including a pan edited moments ago.
    const QList<SpectrumWidget*> pans = panWidgets();
    for (SpectrumWidget* pan : pans) {
        pan->storePendingPerPanSetting(keyBase);
    }
    // The default first, then the pans: see the header.
    PanDisplayKeys::writeDefault(keyBase, value);
    if (apply) {
        for (SpectrumWidget* pan : pans) {
            if (!PanDisplayKeys::hasOverride(keyBase, pan->panIndex())) {
                apply(pan);
            }
        }
    }
    if (m_model && keyBase == QStringLiteral("ClarityEnabled")) {
        m_model->setClarityEnabled(AppSettings::instance().value(QStringLiteral("ClarityEnabled")).toString()
                                  == QStringLiteral("True"));
    }
    const QList<SetupPage*> pages = displayFollowerPages();
    for (SetupPage* page : pages) {
        if (page != this) { page->showDisplayDefault(keyBase); }
    }
    refreshDisplayPanScope();
}

QString SetupPage::displayDefaultText(const QString& keyBase) const
{
    if (m_displayPanIndex >= 0 && !isSharedDisplaySetting(keyBase)) {
        if (SpectrumWidget* pan = selectedDisplayPan()) { return pan->perPanDisplayValue(keyBase); }
        const QList<SpectrumWidget*> pans = panWidgets();
        const QString fallback = pans.isEmpty() ? SpectrumWidget::perPanShipDefaultForClosedPan(keyBase)
                                                     : pans.first()->perPanShipDefault(keyBase);
        return PanDisplayKeys::read(keyBase, m_displayPanIndex, fallback).toString();
    }
    const QString stored =
        AppSettings::instance().value(PanDisplayKeys::defaultKey(keyBase)).toString();
    if (!stored.isEmpty()) { return stored; }
    const QList<SpectrumWidget*> pans = panWidgets();
    return pans.isEmpty() ? SpectrumWidget::perPanShipDefaultForClosedPan(keyBase) : pans.first()->perPanShipDefault(keyBase);
}

void SetupPage::followDisplayDefault(const QString& keyBase, std::function<void()> show)
{
    if (m_displayFollowers.isEmpty()) {
        displayFollowerPages().append(this);
    }
    const std::function<void()> existing = m_displayFollowers.value(keyBase);
    if (existing) {
        m_displayFollowers.insert(keyBase, [existing, show = std::move(show)]() {
            existing();
            if (show) { show(); }
        });
    } else {
        m_displayFollowers.insert(keyBase, std::move(show));
    }
}

void SetupPage::showDisplayDefault(const QString& keyBase)
{
    const auto follower = m_displayFollowers.constFind(keyBase);
    if (follower != m_displayFollowers.constEnd() && *follower) {
        (*follower)();
    }
    // Effective levels are a runtime-only picture notification; they do
    // not have a saved default follower but still refresh readouts/gates.
    displayPansChanged();
    refreshDisplayPanScope();
}

void SetupPage::showDisplayDefaults()
{
    followPanWidgets();
    for (const std::function<void()>& show : std::as_const(m_displayFollowers)) {
        if (show) { show(); }
    }
    refreshDisplayPanScope();
}

void SetupPage::displayPansChanged()
{
}

void SetupPage::setFallbackPan(SpectrumWidget* pan)
{
    m_fallbackPan = pan;
}

void SetupPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    refreshDisplayPanScope();
    refreshDisplayBandPicker();
    if (m_displayFollowers.isEmpty()) { return; }
    // Pans come and go while Setup is closed or on another page.
    followPanWidgets();
    showDisplayDefaults();
    displayPansChanged();
}

void SetupPage::followPanWidgets()
{
    if (m_displayFollowers.isEmpty()) { return; }
    for (SpectrumWidget* pan : panWidgets()) {
        if (m_followedPans.contains(pan)) { continue; }
        m_followedPans.insert(pan);
        connect(pan, &QObject::destroyed, this,
                [this](QObject* gone) { m_followedPans.remove(gone); });
        connect(pan, &SpectrumWidget::displaySettingChanged, this,
                [this](const QString& keyBase) { showDisplayDefault(keyBase); });
        connect(pan, &SpectrumWidget::panDisplayReset, this, [this]() {
            showDisplayDefaults();
            displayPansChanged();
        });
        connect(pan, &SpectrumWidget::clarityActiveChanged, this,
                [this](bool) { displayPansChanged(); });
        connect(pan, &SpectrumWidget::txWfSettingsChanged, this, [this]() {
            for (const QString& key : {QStringLiteral("DisplayTxWfPalette"), QStringLiteral("DisplayTxWfLowColor"),
                                       QStringLiteral("DisplayTxWfLowLevel"), QStringLiteral("DisplayTxWfHighLevel"),
                                       QStringLiteral("DisplayTxWfGradient")}) {
                showDisplayDefault(key);
            }
        });
        // The keys the pan's DisplaySettingsModel holds are announced by the
        // model's own signals.
        DisplaySettingsModel* settings = pan->displaySettings();
        const auto follow = [this, settings](auto signal, const char* keyBase) {
            const QString key = QString::fromLatin1(keyBase);
            connect(settings, signal, this, [this, key]() { showDisplayDefault(key); });
        };
        follow(&DisplaySettingsModel::wfColorSchemeChanged, "DisplayWfColorScheme");
        follow(&DisplaySettingsModel::wfColorGainChanged, "DisplayWfColorGain");
        follow(&DisplaySettingsModel::wfBlackLevelChanged, "DisplayWfBlackLevel");
        follow(&DisplaySettingsModel::fillAlphaChanged, "DisplayFftFillAlpha");
        follow(&DisplaySettingsModel::panFillChanged, "DisplayPanFill");
        follow(&DisplaySettingsModel::spectrumFracChanged, "DisplaySpectrumFrac");
        follow(&DisplaySettingsModel::spectrumRenderModeChanged, "DisplaySpectrumRenderMode");
        follow(&DisplaySettingsModel::dssGainChanged, "Display3DGain");
        follow(&DisplaySettingsModel::dssRowSpanChanged, "Display3DSpan");
        follow(&DisplaySettingsModel::dssAngleChanged, "Display3DAngle");
        follow(&DisplaySettingsModel::dssRowDividerChanged, "Display3DSpeed");
        follow(&DisplaySettingsModel::threeDSliceDepthChanged, "Display3DSliceShadow");
    }
}

void SetupPage::bindDefaultToggle(QCheckBox* box, const QString& keyBase,
                                  std::function<void(SpectrumWidget*, bool)> apply)
{
    box->setProperty("nereusDisplayKeyBase", keyBase);
    connect(box, &QCheckBox::toggled, this, [this, keyBase, apply](bool on) {
        applyDisplayDefault(keyBase, defaultBoolText(on),
                            [&apply, on](SpectrumWidget* pan) { apply(pan, on); });
    });
    followDisplayDefault(keyBase, [this, box, keyBase]() {
        const QString text = displayDefaultText(keyBase);
        if (text.isEmpty()) { return; }
        QSignalBlocker block(box);
        box->setChecked(text == QStringLiteral("True"));
    });
}

void SetupPage::bindDefaultCombo(QComboBox* combo, const QString& keyBase,
                                 std::function<void(SpectrumWidget*, int)> apply)
{
    combo->setProperty("nereusDisplayKeyBase", keyBase);
    connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this, keyBase, apply](int index) {
        applyDisplayDefault(keyBase, QString::number(index),
                            [&apply, index](SpectrumWidget* pan) { apply(pan, index); });
    });
    followDisplayDefault(keyBase, [this, combo, keyBase]() {
        bool ok = false;
        const int index = displayDefaultText(keyBase).toInt(&ok);
        if (!ok) { return; }
        QSignalBlocker block(combo);
        combo->setCurrentIndex(qBound(0, index, combo->count() - 1));
    });
}

void SetupPage::bindDefaultSlider(QSlider* slider, QSpinBox* spin, const QString& keyBase,
                                  std::function<void(SpectrumWidget*, int)> apply)
{
    slider->setProperty("nereusDisplayKeyBase", keyBase);
    if (spin) { spin->setProperty("nereusDisplayKeyBase", keyBase); }
    connect(slider, &QSlider::valueChanged, this, [this, keyBase, apply](int value) {
        applyDisplayDefault(keyBase, QString::number(value),
                            [&apply, value](SpectrumWidget* pan) { apply(pan, value); });
    });
    followDisplayDefault(keyBase, [this, slider, spin, keyBase]() {
        bool ok = false;
        // A whole number, or a stored float ("-62.5") rounded to one.
        const int value = qRound(displayDefaultText(keyBase).toDouble(&ok));
        if (!ok) { return; }
        QSignalBlocker blockSlider(slider);
        slider->setValue(value);
        if (spin) {
            // The slider is quiet, so its number cell is set here too.
            QSignalBlocker blockSpin(spin);
            spin->setValue(slider->value());
        }
    });
}

void SetupPage::bindDefaultSpin(QSpinBox* spin, const QString& keyBase,
                                std::function<void(SpectrumWidget*, int)> apply)
{
    spin->setProperty("nereusDisplayKeyBase", keyBase);
    connect(spin, qOverload<int>(&QSpinBox::valueChanged), this,
            [this, keyBase, apply](int value) {
        applyDisplayDefault(keyBase, QString::number(value),
                            [&apply, value](SpectrumWidget* pan) { apply(pan, value); });
    });
    followDisplayDefault(keyBase, [this, spin, keyBase]() {
        bool ok = false;
        const int value = qRound(displayDefaultText(keyBase).toDouble(&ok));
        if (!ok) { return; }
        QSignalBlocker block(spin);
        spin->setValue(value);
        if (auto* slider = qobject_cast<QSlider*>(spin->property("nereusPairedNumberSlider").value<QObject*>())) {
            const qreal storedScale = spin->property("nereusPairedNumberScale").toDouble();
            const qreal scale = storedScale > 0.0 ? storedScale : 1.0;
            const QSignalBlocker sliderBlock(slider);
            slider->setValue(qRound(spin->value() * scale));
        }
    });
}

namespace {

// One save/restore routine, two property sets: a control follows one gate.
// A page whose control is held for both transmit and the Core combines the
// two itself and calls one helper (AudioTxInputPage does).
void gateControlsWith(const QList<QWidget*>& controls, bool allowed, const QString& reason,
                      const char* savedTooltip, const char* savedDescription,
                      const char* savedEnabled)
{
    for (QWidget* control : controls) {
        if (!control) { continue; }
        if (!allowed) {
            if (!control->property(savedTooltip).isValid()) {
                control->setProperty(savedTooltip, control->toolTip());
                control->setProperty(savedDescription, control->accessibleDescription());
                // The control's own state, not the one its parent imposes:
                // a control inside a greyed section (Audio > Microphone's
                // radio mic groups, R-SPK-21) comes back live when the
                // section is picked, instead of staying disabled for good.
                control->setProperty(savedEnabled,
                                     !control->testAttribute(Qt::WA_ForceDisabled));
            }
            control->setEnabled(false);
            control->setToolTip(reason);
            control->setAccessibleDescription(reason);
        } else if (control->property(savedTooltip).isValid()) {
            control->setEnabled(control->property(savedEnabled).toBool());
            control->setToolTip(control->property(savedTooltip).toString());
            control->setAccessibleDescription(control->property(savedDescription).toString());
            control->setProperty(savedTooltip, QVariant());
            control->setProperty(savedDescription, QVariant());
            control->setProperty(savedEnabled, QVariant());
        }
    }
}

} // namespace

void SetupPage::gateTransmitControls(const QList<QWidget*>& controls, bool permitted,
                                     const QString& reason)
{
    gateControlsWith(controls, permitted, reason, "SetupPageSavedTransmitTooltip",
                     "SetupPageSavedTransmitDescription", "SetupPageSavedTransmitEnabled");
}

void SetupPage::gateStationControls(const QList<QWidget*>& controls, bool available,
                                    const QString& reason)
{
    gateControlsWith(controls, available, reason, "SetupPageSavedStationTooltip",
                     "SetupPageSavedStationDescription", "SetupPageSavedStationEnabled");
}

void SetupPage::gateOwnedControls(const QList<QWidget*>& controls, bool live,
                                  const QString& reason)
{
    gateControlsWith(controls, live, reason, "SetupPageSavedOwnedTooltip",
                     "SetupPageSavedOwnedDescription", "SetupPageSavedOwnedEnabled");
}

void SetupPage::gateOnAirControls(const QList<QWidget*>& controls, bool offAir,
                                  const QString& reason)
{
    gateControlsWith(controls, offAir, reason, "SetupPageSavedOnAirTooltip",
                     "SetupPageSavedOnAirDescription", "SetupPageSavedOnAirEnabled");
}

QGroupBox* SetupPage::addSection(const QString& title)
{
    auto* group = new QGroupBox(title);
    group->setStyleSheet(kGroupStyle);

    auto* groupLayout = new QVBoxLayout(group);
    groupLayout->setContentsMargins(8, 4, 8, 8);
    groupLayout->setSpacing(4);

    m_activeSectionLayout = groupLayout;

    // Insert before the trailing stretch
    const int stretchIndex = m_contentLayout->count() - 1;
    m_contentLayout->insertWidget(stretchIndex, group);

    return group;
}

// R-SPK-21: content goes before the trailing stretch, as addSection() does.
void SetupPage::addContent(QWidget* widget)
{
    const int stretchIndex = m_contentLayout->count() - 1;
    m_contentLayout->insertWidget(stretchIndex, widget);
}

void SetupPage::addContent(QLayout* layout)
{
    const int stretchIndex = m_contentLayout->count() - 1;
    m_contentLayout->insertLayout(stretchIndex, layout);
}

// ── Convenience single-argument row builders ──────────────────────────────────

QPushButton* SetupPage::addLabeledToggle(const QString& label)
{
    auto* btn = new QPushButton;
    btn->setCheckable(true);
    QLayout* target = m_activeSectionLayout ? m_activeSectionLayout : m_contentLayout;
    addLabeledToggle(target, label, btn);
    return btn;
}

QComboBox* SetupPage::addLabeledCombo(const QString& label, const QStringList& items)
{
    auto* combo = new QComboBox;
    combo->addItems(items);
    QLayout* target = m_activeSectionLayout ? m_activeSectionLayout : m_contentLayout;
    addLabeledCombo(target, label, combo);
    return combo;
}

QSlider* SetupPage::addLabeledSlider(const QString& label, int minimum, int maximum, int value)
{
    auto* slider = new QSlider(Qt::Horizontal);
    slider->setRange(minimum, maximum);
    slider->setValue(value);
    QLayout* target = m_activeSectionLayout ? m_activeSectionLayout : m_contentLayout;
    addLabeledSlider(target, label, slider);
    return slider;
}

QSpinBox* SetupPage::addLabeledSpinner(const QString& label, int minimum, int maximum, int value)
{
    auto* spin = new QSpinBox;
    spin->setRange(minimum, maximum);
    spin->setValue(value);
    QLayout* target = m_activeSectionLayout ? m_activeSectionLayout : m_contentLayout;
    addLabeledSpinner(target, label, spin);
    return spin;
}

QPushButton* SetupPage::addLabeledButton(const QString& label, const QString& buttonText)
{
    auto* btn = new QPushButton(buttonText);
    btn->setAutoDefault(false);
    QLayout* target = m_activeSectionLayout ? m_activeSectionLayout : m_contentLayout;
    addLabeledToggle(target, label, btn);  // reuse row layout
    // Re-style as a plain button (not a toggle)
    btn->setCheckable(false);
    btn->setStyleSheet(Style::kButtonStyle);
    return btn;
}

QLabel* SetupPage::addLabeledLabel(const QString& label, const QString& value)
{
    auto* lbl = new QLabel(value);
    QLayout* target = m_activeSectionLayout ? m_activeSectionLayout : m_contentLayout;
    addLabeledLabel(target, label, lbl);
    return lbl;
}

QLineEdit* SetupPage::addLabeledEdit(const QString& label, const QString& placeholder)
{
    auto* edit = new QLineEdit;
    if (!placeholder.isEmpty()) { edit->setPlaceholderText(placeholder); }
    QLayout* target = m_activeSectionLayout ? m_activeSectionLayout : m_contentLayout;
    addLabeledEdit(target, label, edit);
    return edit;
}

// ── Private row builder ───────────────────────────────────────────────────────

QHBoxLayout* SetupPage::makeLabeledRow(QLayout* parent, const QString& labelText, QWidget* control)
{
    auto* row = new QHBoxLayout;
    row->setSpacing(8);

    auto* label = new QLabel(labelText);
    label->setStyleSheet(kLabelStyle);
    // D25: a label longer than the column widens its row, not cut short.
    SetupFit::widthAtLeast(label, 150);
    row->addWidget(label);
    row->addWidget(control, 1);

    // QVBoxLayout and QHBoxLayout both inherit QBoxLayout
    if (auto* box = qobject_cast<QBoxLayout*>(parent)) {
        box->addLayout(row);
    }

    return row;
}

// ── Public helper methods (pre-created control variants) ──────────────────────

QHBoxLayout* SetupPage::addLabeledCombo(QLayout* parent, const QString& label, QComboBox* combo)
{
    combo->setStyleSheet(Style::kComboStyle);
    return makeLabeledRow(parent, label, combo);
}

QHBoxLayout* SetupPage::addLabeledSlider(QLayout* parent, const QString& label,
                                          QSlider* slider, QLabel* valueLabel)
{
    slider->setStyleSheet(Style::kSliderStyle);

    if (valueLabel != nullptr) {
        valueLabel->setStyleSheet("QLabel { color: #00c8ff; font-size: 12px; "
                                  "font-weight: bold; min-width: 40px; }");

        auto* row = new QHBoxLayout;
        row->setSpacing(8);

        auto* lbl = new QLabel(label);
        lbl->setStyleSheet(kLabelStyle);
        SetupFit::widthAtLeast(lbl, 150);
        row->addWidget(lbl);
        row->addWidget(slider, 1);
        row->addWidget(valueLabel);

        if (auto* box = qobject_cast<QBoxLayout*>(parent)) {
            box->addLayout(row);
        }
        return row;
    }

    return makeLabeledRow(parent, label, slider);
}

QHBoxLayout* SetupPage::addLabeledToggle(QLayout* parent, const QString& label, QPushButton* toggle)
{
    toggle->setCheckable(true);
    toggle->setStyleSheet(
        "QPushButton { background: #1a2a3a; border: 1px solid #304050; "
        "border-radius: 3px; color: #c8d8e8; font-size: 11px; font-weight: bold; "
        "padding: 3px 10px; }"
        "QPushButton:checked { background: #1a5030; color: #00e060; "
        "border: 1px solid #20a040; }");
    return makeLabeledRow(parent, label, toggle);
}

QHBoxLayout* SetupPage::addLabeledSpinner(QLayout* parent, const QString& label, QSpinBox* spinner)
{
    // No setStyleSheet — matches the AudioAdvancedPage pattern (native
    // QSpinBox styling on macOS has proper up/down buttons). Per user
    // directive 2026-04-23: Audio → Advanced is the canonical style for
    // spinbox-type fields across the Setup dialog.
    return makeLabeledRow(parent, label, spinner);
}

QHBoxLayout* SetupPage::addLabeledEdit(QLayout* parent, const QString& label, QLineEdit* edit)
{
    edit->setStyleSheet(Style::kLineEditStyle);
    return makeLabeledRow(parent, label, edit);
}

QHBoxLayout* SetupPage::addLabeledLabel(QLayout* parent, const QString& label, QLabel* value)
{
    value->setStyleSheet("QLabel { color: #00c8ff; font-size: 12px; font-weight: bold; }");
    return makeLabeledRow(parent, label, value);
}

} // namespace NereusSDR
