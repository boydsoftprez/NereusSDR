// Modification history (NereusSDR):
//   2026-10-26 - PR 677 review fix wave (I4): the next 4-char ID field
//                 accepts only letters and digits (webImageNextFourChar).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Colour swatches show the colour drawn and say when it
//                 follows the workspace look (ItemColourLook::paintSwatch).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Web image settings after the Thetis setup page: preset
//                 lists (HamQSL, BSDWorld with Dark Mode, NASA, NOAA),
//                 refresh 1-7200 s, width scale, bypass cache, the
//                 known-host lock (interval and bypass disabled), the
//                 fetch state and the 4-char ID with a Copy button. The
//                 rules live in WebImageItem; this page only shows them.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Background: the pan the image is shown on, how long it
//                 stays, the next web image's 4-char ID and Show next,
//                 after the Thetis setup page. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.

#include "WebImageItemEditor.h"
#include "gui/meters/ItemColourLook.h"
#include "../../meters/WebImageItem.h"
#include "core/AppSettings.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSpinBox>

#include <algorithm>

namespace NereusSDR {

namespace {
// The BSDWorld Dark Mode choice, kept across sessions.
constexpr const char* kBsdDarkKey = "WebImageBsdWorldDarkMode";

constexpr const char* kLineStyle =
    "QLineEdit { background: #0a0a18; color: #c8d8e8;"
    " border: 1px solid #1e2e3e; border-radius: 3px;"
    " padding: 1px 4px; min-height: 18px; }";

constexpr const char* kComboStyle =
    "QComboBox {"
    "  background: #0a0a18; color: #c8d8e8;"
    "  border: 1px solid #1e2e3e; border-radius: 3px;"
    "  padding: 2px 4px; min-height: 18px;"
    "}"
    "QComboBox QAbstractItemView {"
    "  background: #0a0a18; color: #c8d8e8;"
    "  border: 1px solid #205070;"
    "  selection-background-color: #00b4d8;"
    "}";

void applyBtnColor(QPushButton* btn, const QString& field, const QColor& c)
{
    ItemColourLook::paintSwatch(btn, field, c);
}
} // namespace

WebImageItemEditor::WebImageItemEditor(QWidget* parent)
    : BaseItemEditor(parent)
{
    buildTypeSpecific();
}

void WebImageItemEditor::setItem(MeterItem* item)
{
    BaseItemEditor::setItem(item);
    QObject::disconnect(m_stateConnection);
    WebImageItem* w = qobject_cast<WebImageItem*>(item);
    if (!w) { return; }

    m_stateConnection = connect(w, &WebImageItem::stateChanged,
                                this, &WebImageItemEditor::refreshState);

    beginProgrammaticUpdate();
    m_editUrl->setText(w->url());
    m_spinRefresh->setValue(w->refreshInterval());
    m_spinWidthScale->setValue(w->widthScale());
    m_chkBypassCache->setChecked(w->bypassCache());
    m_editFourChar->setText(w->fourChar());
    applyBtnColor(m_btnFallback, QStringLiteral("WebImage.fallbackColor"), w->fallbackColor());
    m_comboPan->setCurrentIndex(std::max(0, m_comboPan->findData(w->targetPan())));
    m_spinBackgroundSecs->setValue(w->backgroundIntervalSecs());
    m_editNextFourChar->setText(w->backgroundFourChar());
    endProgrammaticUpdate();
    refreshLockedControls();
    refreshState();
    refreshBackgroundControls();
}

// The interval, next ID and Show next apply only while a pan is chosen.
void WebImageItemEditor::refreshBackgroundControls()
{
    const bool on = m_comboPan->currentData().toInt() != kWebImageNoPan;
    m_spinBackgroundSecs->setEnabled(on);
    m_editNextFourChar->setEnabled(on);
    m_btnShowNext->setEnabled(on && !m_editNextFourChar->text().trimmed().isEmpty());
}

void WebImageItemEditor::applyUrl(const QString& text)
{
    WebImageItem* w = qobject_cast<WebImageItem*>(m_item);
    if (!w) { return; }
    QString url = text.trimmed();
    // An old BSDWorld preset URL gets -light or -dark inserted.
    const QString bsd = WebImageItem::fixBsdWorldUrl(url, m_chkBsdDark->isChecked());
    if (!bsd.isEmpty()) { url = bsd; }
    if (m_editUrl->text() != url) {
        beginProgrammaticUpdate();
        m_editUrl->setText(url);
        endProgrammaticUpdate();
    }
    w->setUrl(url); // also fetches
    refreshLockedControls();
    notifyChanged();
}

void WebImageItemEditor::refreshLockedControls()
{
    WebImageItem* w = qobject_cast<WebImageItem*>(m_item);
    if (!w) { return; }
    const bool locked = WebImageItem::isLockedHost(w->url());
    beginProgrammaticUpdate();
    m_spinRefresh->setValue(w->refreshInterval());
    m_chkBypassCache->setChecked(w->bypassCache());
    endProgrammaticUpdate();
    m_spinRefresh->setEnabled(!locked);
    m_chkBypassCache->setEnabled(!locked);
    const QString why = locked
        ? QStringLiteral("Fixed at 600 s with no cache bypass for this site")
        : QString();
    m_spinRefresh->setToolTip(why);
    m_chkBypassCache->setToolTip(why);
}

void WebImageItemEditor::refreshState()
{
    WebImageItem* w = qobject_cast<WebImageItem*>(m_item);
    m_lblState->setText(w ? w->stateText() : QString());
}

void WebImageItemEditor::buildTypeSpecific()
{
    addHeader(QStringLiteral("Web Image"));

    // URL + reload button in a horizontal layout
    auto* rowWidget = new QWidget(this);
    auto* rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(4);

    m_editUrl = new QLineEdit(rowWidget);
    m_editUrl->setStyleSheet(QString::fromLatin1(kLineStyle));
    m_editUrl->setToolTip(QStringLiteral("An image URL, or a file:// path to a local image"));
    rowLayout->addWidget(m_editUrl, 1);

    auto* btnReload = new QPushButton(QStringLiteral("Reload"), rowWidget);
    btnReload->setFixedWidth(48);
    rowLayout->addWidget(btnReload);

    addRow(QStringLiteral("URL"), rowWidget);

    connect(m_editUrl, &QLineEdit::editingFinished, this, [this]() {
        if (isProgrammaticUpdate()) { return; }
        applyUrl(m_editUrl->text());
    });
    connect(btnReload, &QPushButton::clicked, this, [this]() {
        applyUrl(m_editUrl->text()); // triggers a fetch
    });

    // Preset lists: picking one sets the URL, then the list returns to
    // "select one".
    m_chkBsdDark = new QCheckBox(QStringLiteral("Dark Mode"), this);
    m_chkBsdDark->setChecked(
        AppSettings::instance().value(QString::fromLatin1(kBsdDarkKey), QStringLiteral("False"))
            .toString() == QLatin1String("True"));
    connect(m_chkBsdDark, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QString::fromLatin1(kBsdDarkKey),
                                         on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    for (const WebImagePresetGroup& group : WebImageItem::presetGroups()) {
        auto* combo = new QComboBox(this);
        combo->setStyleSheet(QString::fromLatin1(kComboStyle));
        combo->addItem(QStringLiteral("select one"));
        for (const WebImagePreset& preset : group.presets) {
            combo->addItem(preset.name, preset.url);
        }
        connect(combo, qOverload<int>(&QComboBox::activated), this, [this, combo](int index) {
            if (index <= 0) { return; }
            const QString url = WebImageItem::resolveLightMode(
                combo->itemData(index).toString(), m_chkBsdDark->isChecked());
            combo->setCurrentIndex(0);
            applyUrl(url);
        });
        m_presetCombos.append(combo);
        if (group.name == QLatin1String("BSDWorld")) {
            auto* bsdRow = new QWidget(this);
            auto* bsdLayout = new QHBoxLayout(bsdRow);
            bsdLayout->setContentsMargins(0, 0, 0, 0);
            bsdLayout->setSpacing(4);
            bsdLayout->addWidget(combo, 1);
            bsdLayout->addWidget(m_chkBsdDark);
            addRow(group.name, bsdRow);
        } else {
            addRow(group.name, combo);
        }
    }

    // Refresh interval
    m_spinRefresh = makeIntRow(QStringLiteral("Refresh (s)"),
                               kWebImageMinIntervalSecs, kWebImageMaxIntervalSecs);
    connect(m_spinRefresh, qOverload<int>(&QSpinBox::valueChanged),
            this, [this](int v) {
        if (isProgrammaticUpdate()) { return; }
        WebImageItem* w = qobject_cast<WebImageItem*>(m_item);
        if (!w) { return; }
        w->setRefreshInterval(v);
        notifyChanged();
    });

    // Width scale: 1.0 fills the item width.
    m_spinWidthScale = makeDoubleRow(QStringLiteral("Width scale"),
                                     kWebImageMinWidthScale, kWebImageMaxWidthScale,
                                     kWebImageWidthScaleStep, kWebImageWidthScaleDecimals);
    m_spinWidthScale->setToolTip(QStringLiteral("1.0 fills the item width"));
    connect(m_spinWidthScale, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (isProgrammaticUpdate()) { return; }
        WebImageItem* w = qobject_cast<WebImageItem*>(m_item);
        if (!w) { return; }
        w->setWidthScale(v);
        notifyChanged();
    });

    // Bypass cache
    m_chkBypassCache = makeCheckRow(QStringLiteral("Bypass cache"));
    connect(m_chkBypassCache, &QCheckBox::toggled, this, [this](bool on) {
        if (isProgrammaticUpdate()) { return; }
        WebImageItem* w = qobject_cast<WebImageItem*>(m_item);
        if (!w) { return; }
        w->setBypassCache(on);
        notifyChanged();
    });

    // Fetch state
    m_lblState = new QLabel(this);
    m_lblState->setStyleSheet(QStringLiteral("QLabel { color: #c8d8e8; }"));
    addRow(QStringLiteral("State"), m_lblState);

    // 4-char ID, read only, with Copy
    auto* idRow = new QWidget(this);
    auto* idLayout = new QHBoxLayout(idRow);
    idLayout->setContentsMargins(0, 0, 0, 0);
    idLayout->setSpacing(4);
    m_editFourChar = new QLineEdit(idRow);
    m_editFourChar->setReadOnly(true);
    m_editFourChar->setMaxLength(4);
    m_editFourChar->setFixedWidth(52);
    m_editFourChar->setStyleSheet(QString::fromLatin1(kLineStyle));
    m_editFourChar->setToolTip(QStringLiteral("The four character code for this web image"));
    idLayout->addWidget(m_editFourChar);
    auto* btnCopy = new QPushButton(QStringLiteral("Copy"), idRow);
    btnCopy->setFixedWidth(48);
    idLayout->addWidget(btnCopy);
    idLayout->addStretch(1);
    addRow(QStringLiteral("4-char ID"), idRow);
    connect(btnCopy, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_editFourChar->text());
    });

    // Fallback color
    m_btnFallback = new QPushButton(this);
    m_btnFallback->setFixedSize(40, 18);
    connect(m_btnFallback, &QPushButton::clicked, this, [this]() {
        WebImageItem* w = qobject_cast<WebImageItem*>(m_item);
        if (!w) { return; }
        const QColor chosen = QColorDialog::getColor(
            w->fallbackColor(), this, QString(), QColorDialog::ShowAlphaChannel);
        if (chosen.isValid()) {
            markColourPicked(QStringLiteral("WebImage.fallbackColor"));
            w->setFallbackColor(chosen);
            applyBtnColor(m_btnFallback, QStringLiteral("WebImage.fallbackColor"), chosen);
            notifyChanged();
        }
    });
    addRow(QStringLiteral("Fallback color"), m_btnFallback);

    // Background: the pan whose spectrum shows this image.
    addHeader(QStringLiteral("Background"));
    m_comboPan = new QComboBox(this);
    m_comboPan->setStyleSheet(QString::fromLatin1(kComboStyle));
    m_comboPan->addItem(QStringLiteral("None"), kWebImageNoPan);
    for (int pan = 0; pan < kWebImageMaxPans; ++pan) {
        m_comboPan->addItem(QStringLiteral("Pan %1").arg(pan + 1), pan);
    }
    m_comboPan->setToolTip(QStringLiteral("Show this image behind the spectrum of a pan"));
    addRow(QStringLiteral("Show on"), m_comboPan);
    connect(m_comboPan, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        refreshBackgroundControls();
        if (isProgrammaticUpdate()) { return; }
        WebImageItem* w = qobject_cast<WebImageItem*>(m_item);
        if (!w) { return; }
        w->setTargetPan(m_comboPan->currentData().toInt());
        notifyChanged();
    });

    m_spinBackgroundSecs = makeIntRow(QStringLiteral("Next after (s)"),
                                      kWebImageMinBackgroundSecs, kWebImageMaxBackgroundSecs);
    m_spinBackgroundSecs->setToolTip(
        QStringLiteral("How long this image stays before the next one is shown"));
    connect(m_spinBackgroundSecs, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        if (isProgrammaticUpdate()) { return; }
        WebImageItem* w = qobject_cast<WebImageItem*>(m_item);
        if (!w) { return; }
        w->setBackgroundIntervalSecs(v);
        notifyChanged();
    });

    auto* nextRow = new QWidget(this);
    auto* nextLayout = new QHBoxLayout(nextRow);
    nextLayout->setContentsMargins(0, 0, 0, 0);
    nextLayout->setSpacing(4);
    m_editNextFourChar = new QLineEdit(nextRow);
    m_editNextFourChar->setObjectName(QStringLiteral("webImageNextFourChar"));
    m_editNextFourChar->setMaxLength(4);
    // The 4-char ID alphabet (WebImageItem::setBackgroundFourChar keeps
    // only these too).
    m_editNextFourChar->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[A-Za-z0-9]{0,4}")), m_editNextFourChar));
    m_editNextFourChar->setFixedWidth(52);
    m_editNextFourChar->setStyleSheet(QString::fromLatin1(kLineStyle));
    m_editNextFourChar->setToolTip(
        QStringLiteral("The 4-char ID of the web image shown next on this pan"));
    nextLayout->addWidget(m_editNextFourChar);
    m_btnShowNext = new QPushButton(QStringLiteral("Show next"), nextRow);
    m_btnShowNext->setToolTip(QStringLiteral("Open the settings of the next web image"));
    nextLayout->addWidget(m_btnShowNext);
    nextLayout->addStretch(1);
    addRow(QStringLiteral("Next 4-char ID"), nextRow);
    connect(m_editNextFourChar, &QLineEdit::textChanged, this, [this](const QString&) {
        refreshBackgroundControls();
    });
    connect(m_editNextFourChar, &QLineEdit::editingFinished, this, [this]() {
        if (isProgrammaticUpdate()) { return; }
        WebImageItem* w = qobject_cast<WebImageItem*>(m_item);
        if (!w) { return; }
        w->setBackgroundFourChar(m_editNextFourChar->text());
        notifyChanged();
    });
    connect(m_btnShowNext, &QPushButton::clicked, this, [this]() {
        const QString next = m_editNextFourChar->text().trimmed();
        if (!next.isEmpty()) { emit showNextRequested(next); }
    });
    refreshBackgroundControls();
}

} // namespace NereusSDR
