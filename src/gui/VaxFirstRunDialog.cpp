// =================================================================
// src/gui/VaxFirstRunDialog.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original file — no Thetis port; no attribution-registry row.
//
// Mockup source (palette + copy, byte-verbatim for Mac/Linux explains):
//   .superpowers/brainstorm/64803-1776605394/content/mockup-firstrun.html
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Written by J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Anthropic Claude Code.
// =================================================================

#include "VaxFirstRunDialog.h"
#include "StyleConstants.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <array>

namespace NereusSDR {

namespace {

// Mockup HTML uses the exact widths of 560 px for the dialog shell.
constexpr int kFixedDialogWidth = 560;

// Object-name prefixes so tests can locate specific buttons without
// relying on the child-iteration order of findChildren().
constexpr const char* kBtnApplySuggested = "btnApplySuggested";
constexpr const char* kBtnSkip           = "btnSkip";
constexpr const char* kBtnCustomize      = "btnCustomize";
constexpr const char* kBtnGotIt          = "btnGotIt";
constexpr const char* kBtnInstallPrefix  = "btnInstall_";
constexpr const char* kBtnRescanNow      = "btnRescanNow";
constexpr const char* kBtnWhyNeeded      = "btnWhyNeeded";

// Platform-badge colours — mockup CSS (.plat / .plat.mac / .plat.linux)
// maps byte-for-byte to the shared palette: #00b4d8=kAccent (Windows),
// #00ff88=kGreenText (Mac), #ddbb00=kAmberWarn (Linux). See
// platformBadgeColor() below for the scenario → palette-slot mapping.

// Row styling for the detection list (.det-list / .det-row).
QString detListFrameStyle()
{
    return QStringLiteral(
        "QFrame#detList { background: %1; border: 1px solid %2;"
        " border-radius: 3px; }")
        .arg(Style::kInsetBg, Style::kInsetBorder);
}

QString pillNewStyle()
{
    return QStringLiteral(
        "QLabel { background: %1; border: 1px solid %2; color: %3;"
        " padding: 1px 6px; font-size: 9px; font-weight: bold;"
        " border-radius: 2px; }")
        .arg(Style::kGreenBg, Style::kGreenBorder, Style::kGreenText);
}

QString pillSuggestedStyle()
{
    return QStringLiteral(
        "QLabel { background: %1; border: 1px solid %2; color: %3;"
        " padding: 1px 6px; font-size: 9px; font-weight: bold;"
        " border-radius: 2px; }")
        .arg(Style::kAmberBg, Style::kAmberBorder, Style::kAmberText);
}

QString introLabelStyle()
{
    return QStringLiteral(
        "QLabel { color: %1; font-size: 12px; }")
        .arg(Style::kTextPrimary);
}

QString explainBlockStyle()
{
    return QStringLiteral(
        "QFrame { background: %1; border: 1px solid %2;"
        " border-left: 3px solid %3; border-radius: 3px; }"
        "QLabel { color: %4; font-size: 10px; }"
        "QLabel#explainStrong { color: %5; font-size: 10px; font-weight: bold; }")
        .arg(Style::kInsetBg, Style::kInsetBorder, Style::kAccent,
             Style::kTextSecondary, Style::kTextPrimary);
}

QString primaryButtonStyle()
{
    return QStringLiteral(
        "QPushButton { background: %1; border: 1px solid %2; color: %3;"
        " padding: 5px 14px; font-size: 11px; font-weight: bold;"
        " border-radius: 3px; }"
        "QPushButton:hover { background: %4; }")
        .arg(Style::kBlueBg, Style::kBlueBorder, Style::kBlueText,
             Style::kBlueHover);
}

QString neutralButtonStyle()
{
    return QStringLiteral(
        "QPushButton { background: %1; border: 1px solid %2; color: %3;"
        " padding: 5px 14px; font-size: 11px; font-weight: bold;"
        " border-radius: 3px; }"
        "QPushButton:hover { background: %4; }")
        .arg(Style::kButtonBg, Style::kBorder, Style::kTextPrimary,
             Style::kButtonAltHover);
}

QString linkButtonStyle()
{
    return QStringLiteral(
        "QPushButton { background: transparent; border: none; color: %1;"
        " text-decoration: underline; font-size: 11px;"
        " padding: 5px 6px; }")
        .arg(Style::kAccent);
}

// Construct one row of the detection list. `productLabel` is the small
// all-caps prod line (e.g. "VB-Audio · auto-detected"). Pass an empty
// `pillText` to render the greyed "unassigned" placeholder instead of a
// coloured pill.
QWidget* makeDetRow(int vaxSlot,
                    const QString& deviceLine,
                    const QString& productLabel,
                    const QString& pillText,
                    const QString& pillStyle,
                    const QString& rightGreyText,
                    QWidget* parent)
{
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(14, 8, 14, 8);
    layout->setSpacing(10);

    // Left column — VAX slot number, fixed 70 px per mockup grid-template.
    auto* vaxLabel = new QLabel(
        vaxSlot > 0
            ? QStringLiteral("VAX %1").arg(vaxSlot)
            : QString(),
        row);
    vaxLabel->setFixedWidth(70);
    vaxLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-weight: bold;"
        " font-family: 'ui-monospace','Menlo','Consolas',monospace;"
        " font-size: 11px; }")
        .arg(Style::kTextPrimary));
    layout->addWidget(vaxLabel, 0, Qt::AlignTop);

    // Middle column — device name on top, product line on bottom.
    auto* devBlock = new QWidget(row);
    auto* devLayout = new QVBoxLayout(devBlock);
    devLayout->setContentsMargins(0, 0, 0, 0);
    devLayout->setSpacing(2);

    auto* devLabel = new QLabel(deviceLine, devBlock);
    devLabel->setTextFormat(Qt::RichText);
    devLabel->setWordWrap(true);
    devLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1;"
        " font-family: 'ui-monospace','Menlo','Consolas',monospace;"
        " font-size: 11px; }")
        .arg(Style::kTextSecondary));
    devLayout->addWidget(devLabel);

    if (!productLabel.isEmpty()) {
        auto* prodLabel = new QLabel(productLabel.toUpper(), devBlock);
        prodLabel->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 9px;"
            " letter-spacing: 0.3px; }")
            .arg(Style::kAccent));
        devLayout->addWidget(prodLabel);
    }

    layout->addWidget(devBlock, 1);

    // Right column — either a coloured pill or a small grey right-aligned
    // label. Fixed 90 px per mockup grid.
    auto* rightSide = new QLabel(row);
    rightSide->setFixedWidth(90);
    rightSide->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    if (!pillText.isEmpty()) {
        rightSide->setText(pillText);
        rightSide->setStyleSheet(pillStyle);
        // Let the pill size itself — avoid stretching it the full 90 px.
        rightSide->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    } else {
        rightSide->setText(rightGreyText);
        rightSide->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }")
            .arg(Style::kTextInactive));
    }
    layout->addWidget(rightSide, 0, Qt::AlignTop);

    row->setStyleSheet(QStringLiteral(
        "QWidget { background: transparent; border-bottom: 1px solid %1; }")
        .arg(Style::kInsetBorder));

    return row;
}

QString productDisplayName(VirtualCableProduct p)
{
    switch (p) {
        case VirtualCableProduct::VbCable:
        case VirtualCableProduct::VbCableA:
        case VirtualCableProduct::VbCableB:
        case VirtualCableProduct::VbCableC:
        case VirtualCableProduct::VbCableD:
        case VirtualCableProduct::VbHiFiCable:
            return QStringLiteral("VB-Audio");
        case VirtualCableProduct::MuzychenkoVac:
            return QStringLiteral("Virtual Audio Cable");
        case VirtualCableProduct::Voicemeeter:
            return QStringLiteral("VoiceMeeter");
        case VirtualCableProduct::Dante:
            return QStringLiteral("Dante");
        case VirtualCableProduct::FlexRadioDax:
            return QStringLiteral("FlexRadio DAX");
        case VirtualCableProduct::NereusSdrVax:
            return QStringLiteral("NereusSDR VAX");
        case VirtualCableProduct::None:
        default:
            return QStringLiteral("virtual cable");
    }
}

// Shared detection-row builder for scenarios A (WindowsCablesFound) and
// E (RescanNewCables). Both flow raw `DetectedCable::deviceName` into
// the rich-text QLabel built by makeDetRow(); HTML-escape the
// user-controlled name here so ampersands and angle brackets in vendor
// product names (e.g. FlexRadio's "FLEX-6500 DAX RX1 IN & OUT") don't
// corrupt rendering. `productLineSuffix` is the per-scenario trailing
// text appended after the product-family name (e.g. "auto-detected" for
// A, "newly installed" for E). `pillText` / `pillStyle` encode the
// per-scenario badge (Suggested vs New).
QWidget* makeDetRowForCable(int slot,
                            const DetectedCable& cable,
                            const QString& productLineSuffix,
                            const QString& pillText,
                            const QString& pillStyle,
                            QWidget* parent)
{
    const QString deviceLine = cable.deviceName.toHtmlEscaped();
    const QString prodText = QStringLiteral("%1 \u2022 %2")
        .arg(productDisplayName(cable.product), productLineSuffix);
    return makeDetRow(slot, deviceLine, prodText,
                      pillText, pillStyle,
                      QString(), parent);
}

} // namespace

VaxFirstRunDialog::VaxFirstRunDialog(FirstRunScenario scenario,
                                     const QVector<DetectedCable>& detected,
                                     QWidget* parent)
    : QDialog(parent),
      m_scenario(scenario),
      m_detected(detected)
{
    setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint
                   | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    setWindowTitle(headerTitle());
    setFixedWidth(kFixedDialogWidth);

    setStyleSheet(QStringLiteral(
        "QDialog { background: %1; border: 1px solid %2; }")
        .arg(Style::kAppBg, Style::kOverlayBorder));

    buildUI();

    // Let the body dictate height; width stays pinned.
    layout()->setSizeConstraint(QLayout::SetFixedSize);
}

QVector<QPair<int, QString>> VaxFirstRunDialog::computeSuggestedBindings() const
{
    QVector<QPair<int, QString>> bindings;

    // Scenarios A and E both derive bindings from m_detected. For A we
    // bind starting at VAX 1 in detection order; for E the caller of
    // Task 11b hands us only the NEW cables, so we also bind starting
    // at VAX 1 but the MainWindow hook (Task 11b) is responsible for
    // mapping to the first UNASSIGNED slot. Sub-Phase 11's dialog
    // pre-populates the payload with the detected deviceName list in
    // order; MainWindow does the slot-skip arithmetic.
    if (m_scenario != FirstRunScenario::WindowsCablesFound
        && m_scenario != FirstRunScenario::RescanNewCables) {
        return bindings;
    }

    int channel = 1;
    for (const auto& cable : m_detected) {
        if (channel > 4) {
            break;
        }
        // Only bind input-side devices — VAX consumes a capture endpoint
        // (WSJT-X reads the cable's Output; we write to the cable's
        // Input). matchProduct sees both sides; skip outputs so we don't
        // duplicate VAX bindings for the same underlying cable.
        if (!cable.isInput) {
            continue;
        }
        bindings.append({channel, cable.deviceName});
        ++channel;
    }
    return bindings;
}

void VaxFirstRunDialog::buildUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // ── Header bar ─────────────────────────────────────────────────────
    mainLayout->addWidget(buildHeaderBar());

    // ── Body ──────────────────────────────────────────────────────────
    auto* bodyFrame = new QFrame(this);
    auto* bodyLayout = new QVBoxLayout(bodyFrame);
    bodyLayout->setContentsMargins(18, 16, 18, 16);
    bodyLayout->setSpacing(10);

    switch (m_scenario) {
        case FirstRunScenario::WindowsCablesFound:
            buildBodyWindowsCablesFound(bodyLayout);
            break;
        case FirstRunScenario::WindowsNoCables:
            buildBodyWindowsNoCables(bodyLayout);
            break;
        case FirstRunScenario::MacNative:
            buildBodyMacNative(bodyLayout);
            break;
        case FirstRunScenario::LinuxNative:
            buildBodyLinuxNative(bodyLayout);
            break;
        case FirstRunScenario::RescanNewCables:
            buildBodyRescanNewCables(bodyLayout);
            break;
    }

    mainLayout->addWidget(bodyFrame);

    // ── Footer ────────────────────────────────────────────────────────
    mainLayout->addWidget(buildFooter());
}

QWidget* VaxFirstRunDialog::buildHeaderBar()
{
    auto* header = new QFrame(this);
    header->setObjectName(QStringLiteral("dlgHdr"));
    header->setStyleSheet(QStringLiteral(
        "QFrame#dlgHdr {"
        " background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "  stop:0 %1, stop:0.5 %2, stop:1 %3);"
        " border-bottom: 1px solid %4; }")
        .arg(Style::kTitleGradTop, Style::kTitleGradMid,
             Style::kTitleGradBot, Style::kTitleBorder));

    auto* layout = new QHBoxLayout(header);
    layout->setContentsMargins(14, 8, 14, 8);
    layout->setSpacing(8);

    auto* titleLabel = new QLabel(headerTitle(), header);
    titleLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 13px; font-weight: bold; }")
        .arg(Style::kTextPrimary));
    layout->addWidget(titleLabel);
    layout->addStretch();

    auto* badge = new QLabel(platformBadgeText(), header);
    const QString badgeColor = platformBadgeColor();
    badge->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; background: %2; border: 1px solid %1;"
        " padding: 1px 6px; border-radius: 2px;"
        " font-size: 10px; font-weight: bold; letter-spacing: 0.5px; }")
        .arg(badgeColor, Style::kInsetBg));
    layout->addWidget(badge);

    return header;
}

void VaxFirstRunDialog::buildBodyWindowsCablesFound(QVBoxLayout* bodyLayout)
{
    // Count input-side detections so the intro matches the mockup's
    // "4 virtual audio cables" phrasing without lying when the actual
    // count differs.
    int inputCount = 0;
    for (const auto& c : m_detected) {
        if (c.isInput) {
            ++inputCount;
        }
    }

    auto* intro = new QLabel(this);
    intro->setTextFormat(Qt::RichText);
    intro->setWordWrap(true);
    intro->setStyleSheet(introLabelStyle());
    intro->setText(QStringLiteral(
        "We found <b style='color:%1'>%2 virtual audio cable%3</b> already"
        " installed on your system. Bind them to VAX slots so digital-mode"
        " apps (WSJT-X, FLDIGI, N1MM+) can see your NereusSDR receivers as"
        " audio sources.")
        .arg(Style::kAccent)
        .arg(inputCount)
        .arg(inputCount == 1 ? QStringLiteral("") : QStringLiteral("s")));
    bodyLayout->addWidget(intro);

    // Detection list — fill slots 1..4 from input-side detections in
    // order. Slots without a detection get the "no more cables available"
    // placeholder row matching the mockup.
    auto* listFrame = new QFrame(this);
    listFrame->setObjectName(QStringLiteral("detList"));
    listFrame->setStyleSheet(detListFrameStyle());
    auto* listLayout = new QVBoxLayout(listFrame);
    listLayout->setContentsMargins(0, 4, 0, 4);
    listLayout->setSpacing(0);

    int slot = 1;
    for (const auto& cable : m_detected) {
        if (slot > 4) {
            break;
        }
        if (!cable.isInput) {
            continue;
        }
        listLayout->addWidget(makeDetRowForCable(
            slot, cable,
            QStringLiteral("auto-detected"),
            QStringLiteral("Suggested"), pillSuggestedStyle(),
            listFrame));
        ++slot;
    }

    for (; slot <= 4; ++slot) {
        listLayout->addWidget(makeDetRow(
            slot,
            QStringLiteral("<span style='color:%1'>— no more cables"
                           " available —<br/>"
                           "<span style='font-size:9px'>install another"
                           " cable to enable</span></span>")
                .arg(Style::kTextInactive),
            QString(),
            QString(), QString(),
            QStringLiteral("unassigned"),
            listFrame));
    }

    bodyLayout->addWidget(listFrame);

    // Explain callout — paraphrased from mockup; control-surface copy
    // (NereusSDR-native UX) so no byte-verbatim lock required.
    auto* explain = new QFrame(this);
    explain->setStyleSheet(explainBlockStyle());
    auto* explainLayout = new QVBoxLayout(explain);
    explainLayout->setContentsMargins(12, 9, 12, 9);
    auto* explainLabel = new QLabel(explain);
    explainLabel->setTextFormat(Qt::RichText);
    explainLabel->setWordWrap(true);
    explainLabel->setText(QStringLiteral(
        "<span style='color:%1;font-weight:bold'>What happens next:</span>"
        " WSJT-X (or any digi app) will show \"CABLE-A Output\" in its"
        " audio-device dropdown. Pick it there, then set RX1's"
        " <span style='color:%1;font-weight:bold'>VAX</span> button to"
        " <span style='color:%1;font-weight:bold'>1</span> on the VFO flag."
        " NereusSDR sends RX1 audio to CABLE-A, WSJT-X reads it. Same for"
        " VAX 2/3.")
        .arg(Style::kTextPrimary));
    explainLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; }")
        .arg(Style::kTextSecondary));
    explainLayout->addWidget(explainLabel);
    bodyLayout->addWidget(explain);
}

void VaxFirstRunDialog::buildBodyWindowsNoCables(QVBoxLayout* bodyLayout)
{
    auto* intro = new QLabel(this);
    intro->setTextFormat(Qt::RichText);
    intro->setWordWrap(true);
    intro->setStyleSheet(introLabelStyle());
    intro->setText(QStringLiteral(
        "VAX channels route audio to digital-mode apps (WSJT-X, FLDIGI,"
        " N1MM+). <b style='color:%1'>No virtual audio cables were found</b>"
        " on your system. Install one of these to enable VAX:")
        .arg(Style::kAccent));
    bodyLayout->addWidget(intro);

    // Install-cards grid — 2x2 layout per mockup.
    // TODO(sub-phase-12): per-vendor "Installation guide" link not yet wired.
    // Mockup (mockup-firstrun.html:129,135) shows a secondary link button on
    // the VB-CABLE and VAC cards pointing at an in-app installation
    // walkthrough. Sub-Phase 12's Setup → Audio → VAX page will host the
    // target content; wire this button when that page lands.
    struct VendorCard {
        VirtualCableProduct product;
        const char* title;
        const char* subtitle;
        const char* priceLabel;
        const char* priceColor;
        const char* cta;
    };
    // Order + copy byte-verbatim from mockup HTML.
    static constexpr std::array kVendors = {
        VendorCard{VirtualCableProduct::VbCable,
                   "VB-CABLE",
                   "The standard for ham radio digital modes. Free for"
                   " personal use. Includes 5 cables (base + A+B + C+D"
                   " packs). Signed, mature, actively maintained.",
                   "Free / Donationware", Style::kGreenText,
                   "Open download page  \u2192"},
        VendorCard{VirtualCableProduct::MuzychenkoVac,
                   "Virtual Audio Cable",
                   "Muzychenko's VAC. Commercial, up to 256 dynamic cables,"
                   " custom naming. Best-in-class for pro use. Free demo"
                   " available.",
                   "$15\u2013$50", Style::kAmberText,
                   "Learn more  \u2192"},
        VendorCard{VirtualCableProduct::Voicemeeter,
                   "VoiceMeeter",
                   "Virtual audio mixer with integrated virtual cables."
                   " Good if you also want a software mixer. Includes ASIO"
                   " bridge.",
                   "Free / Donationware", Style::kGreenText,
                   "Open download page  \u2192"},
    };

    auto* gridHost = new QWidget(this);
    auto* gridLayout = new QVBoxLayout(gridHost);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setSpacing(10);

    auto makeCard = [this](const VendorCard& v) -> QWidget* {
        auto* card = new QFrame(this);
        card->setStyleSheet(QStringLiteral(
            "QFrame { background: %1; border: 1px solid %2;"
            " border-radius: 3px; }")
            .arg(Style::kInsetBg, Style::kInsetBorder));
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 12, 12, 12);
        cardLayout->setSpacing(4);

        auto* titleRow = new QWidget(card);
        auto* titleLayout = new QHBoxLayout(titleRow);
        titleLayout->setContentsMargins(0, 0, 0, 0);

        auto* title = new QLabel(QString::fromUtf8(v.title), titleRow);
        title->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 12px; font-weight: bold; }")
            .arg(Style::kTextPrimary));
        titleLayout->addWidget(title);
        titleLayout->addStretch();

        auto* price = new QLabel(QString::fromUtf8(v.priceLabel), titleRow);
        price->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 9px; font-weight: bold;"
            " letter-spacing: 0.3px; }")
            .arg(QString::fromUtf8(v.priceColor)));
        titleLayout->addWidget(price);
        cardLayout->addWidget(titleRow);

        auto* subtitle = new QLabel(QString::fromUtf8(v.subtitle), card);
        subtitle->setWordWrap(true);
        subtitle->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }")
            .arg(Style::kTextSecondary));
        cardLayout->addWidget(subtitle);

        auto* actionRow = new QWidget(card);
        auto* actionLayout = new QHBoxLayout(actionRow);
        actionLayout->setContentsMargins(0, 4, 0, 0);

        auto* openBtn = new QPushButton(QString::fromUtf8(v.cta), actionRow);
        const QString objName = QStringLiteral("%1%2")
            .arg(QString::fromUtf8(kBtnInstallPrefix))
            .arg(static_cast<int>(v.product));
        openBtn->setObjectName(objName);
        openBtn->setStyleSheet(primaryButtonStyle());
        const VirtualCableProduct captured = v.product;
        connect(openBtn, &QPushButton::clicked, this, [this, captured]() {
            emit openInstallUrl(VirtualCableDetector::installUrl(captured));
        });
        actionLayout->addWidget(openBtn);
        actionLayout->addStretch();
        cardLayout->addWidget(actionRow);

        return card;
    };

    // Stack pairs of cards in horizontal rows to match the 2-column grid
    // from the mockup. The final "Already have one?" row is a separate
    // dashed card that spans one column.
    QWidget* row1 = new QWidget(gridHost);
    auto* row1Layout = new QHBoxLayout(row1);
    row1Layout->setContentsMargins(0, 0, 0, 0);
    row1Layout->setSpacing(10);
    row1Layout->addWidget(makeCard(kVendors[0]), 1);
    row1Layout->addWidget(makeCard(kVendors[1]), 1);
    gridLayout->addWidget(row1);

    QWidget* row2 = new QWidget(gridHost);
    auto* row2Layout = new QHBoxLayout(row2);
    row2Layout->setContentsMargins(0, 0, 0, 0);
    row2Layout->setSpacing(10);
    row2Layout->addWidget(makeCard(kVendors[2]), 1);

    // Dashed "Already have one?" card — rescan button.
    auto* dashedCard = new QFrame(row2);
    dashedCard->setStyleSheet(QStringLiteral(
        "QFrame { background: %1; border: 1px dashed %2;"
        " border-radius: 3px; }")
        .arg(Style::kInsetBg, Style::kInsetBorder));
    auto* dashedLayout = new QVBoxLayout(dashedCard);
    dashedLayout->setContentsMargins(12, 12, 12, 12);
    dashedLayout->setSpacing(4);

    auto* dashedTitle = new QLabel(
        QStringLiteral("Already have one?"), dashedCard);
    dashedTitle->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 12px; font-weight: bold; }")
        .arg(Style::kTextSecondary));
    dashedLayout->addWidget(dashedTitle);

    auto* dashedSub = new QLabel(
        QStringLiteral("Close this, install your cable of choice, then"
                       " re-open Setup \u2192 Audio. NereusSDR will detect"
                       " it automatically."),
        dashedCard);
    dashedSub->setWordWrap(true);
    dashedSub->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; }")
        .arg(Style::kTextSecondary));
    dashedLayout->addWidget(dashedSub);

    auto* rescanBtn = new QPushButton(QStringLiteral("Rescan now"), dashedCard);
    rescanBtn->setObjectName(QString::fromUtf8(kBtnRescanNow));
    rescanBtn->setStyleSheet(neutralButtonStyle());
    // Task 11b owns rescan semantics — for Sub-Phase 11 the button is a
    // no-op placeholder so the layout matches the mockup. Wiring lands
    // in MainWindow::checkVaxFirstRun + Setup→Audio→VAX rescan flow.
    dashedLayout->addWidget(rescanBtn);
    dashedLayout->addStretch();

    row2Layout->addWidget(dashedCard, 1);
    gridLayout->addWidget(row2);

    bodyLayout->addWidget(gridHost);

    // Explain block — byte-verbatim from mockup.
    auto* explain = new QFrame(this);
    explain->setStyleSheet(explainBlockStyle());
    auto* explainLayout = new QVBoxLayout(explain);
    explainLayout->setContentsMargins(12, 9, 12, 9);
    auto* explainLabel = new QLabel(explain);
    explainLabel->setTextFormat(Qt::RichText);
    explainLabel->setWordWrap(true);
    explainLabel->setText(QStringLiteral(
        "<span style='color:%1;font-weight:bold'>Why is this required on"
        " Windows?</span> Windows doesn't ship a built-in virtual audio"
        " cable. NereusSDR <i>could</i> bundle its own kernel driver (and"
        " we may, in a future version), but it's a significant engineering"
        " investment. For now, we piggy-back on the mature, widely-used"
        " cables the ham-radio community already trusts. On macOS and"
        " Linux, NereusSDR ships native VAX drivers so no extra install"
        " is needed.")
        .arg(Style::kTextPrimary));
    explainLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; }")
        .arg(Style::kTextSecondary));
    explainLayout->addWidget(explainLabel);
    bodyLayout->addWidget(explain);
}

void VaxFirstRunDialog::buildBodyMacNative(QVBoxLayout* bodyLayout)
{
    // Intro — byte-verbatim from mockup.
    auto* intro = new QLabel(this);
    intro->setTextFormat(Qt::RichText);
    intro->setWordWrap(true);
    intro->setStyleSheet(introLabelStyle());
    intro->setText(QStringLiteral(
        "NereusSDR ships native VAX audio devices on macOS."
        " <b style='color:%1'>All 4 VAX channels are ready to use</b>"
        " &mdash; no extra installation needed.")
        .arg(Style::kAccent));
    bodyLayout->addWidget(intro);

    auto* listFrame = new QFrame(this);
    listFrame->setObjectName(QStringLiteral("detList"));
    listFrame->setStyleSheet(detListFrameStyle());
    auto* listLayout = new QVBoxLayout(listFrame);
    listLayout->setContentsMargins(0, 4, 0, 4);
    listLayout->setSpacing(0);

    // Row 1 has the "NereusSDR TX" tail per mockup; rows 2-4 are the
    // plain "NereusSDR VAX N" line. Device-name copy byte-verbatim.
    for (int slot = 1; slot <= 4; ++slot) {
        QString deviceLine;
        if (slot == 1) {
            deviceLine = QStringLiteral(
                "NereusSDR VAX 1 \u00a0\u2022\u00a0 NereusSDR TX");
        } else {
            deviceLine = QStringLiteral("NereusSDR VAX %1").arg(slot);
        }
        listLayout->addWidget(makeDetRow(
            slot, deviceLine,
            QStringLiteral("CoreAudio HAL \u2022 native"),
            QStringLiteral("Ready"), pillNewStyle(),
            QString(), listFrame));
    }
    bodyLayout->addWidget(listFrame);

    // Explain block — byte-verbatim from mockup, EXCEPT the
    // "Also detected: BlackHole 2ch" trailing line which is omitted for
    // Sub-Phase 11 (we don't detect additional cables beyond our own on
    // Mac yet).
    // TODO(sub-phase-12): wire BlackHole/3rd-party detection into Mac explain block
    auto* explain = new QFrame(this);
    explain->setStyleSheet(explainBlockStyle());
    auto* explainLayout = new QVBoxLayout(explain);
    explainLayout->setContentsMargins(12, 9, 12, 9);
    auto* explainLabel = new QLabel(explain);
    explainLabel->setTextFormat(Qt::RichText);
    explainLabel->setWordWrap(true);
    explainLabel->setText(QStringLiteral(
        "<span style='color:%1;font-weight:bold'>How it works on macOS:</span>"
        " NereusSDR installed a CoreAudio HAL plugin during setup."
        " Open WSJT-X, pick"
        " <span style='color:%1;font-weight:bold'>\"NereusSDR VAX 1\"</span>"
        " as the audio device. Set RX1's VAX button to"
        " <span style='color:%1;font-weight:bold'>1</span> on the VFO flag."
        " Done.")
        .arg(Style::kTextPrimary));
    explainLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; }")
        .arg(Style::kTextSecondary));
    explainLayout->addWidget(explainLabel);
    bodyLayout->addWidget(explain);
}

void VaxFirstRunDialog::buildBodyLinuxNative(QVBoxLayout* bodyLayout)
{
    // Intro — byte-verbatim from mockup.
    auto* intro = new QLabel(this);
    intro->setTextFormat(Qt::RichText);
    intro->setWordWrap(true);
    intro->setStyleSheet(introLabelStyle());
    intro->setText(QStringLiteral(
        "NereusSDR created <b style='color:%1'>4 native VAX audio"
        " sources</b> via PipeWire. All channels are ready &mdash; no"
        " extra installation needed.")
        .arg(Style::kAccent));
    bodyLayout->addWidget(intro);

    auto* listFrame = new QFrame(this);
    listFrame->setObjectName(QStringLiteral("detList"));
    listFrame->setStyleSheet(detListFrameStyle());
    auto* listLayout = new QVBoxLayout(listFrame);
    listLayout->setContentsMargins(0, 4, 0, 4);
    listLayout->setSpacing(0);

    for (int slot = 1; slot <= 4; ++slot) {
        listLayout->addWidget(makeDetRow(
            slot,
            QStringLiteral("NereusSDR VAX %1").arg(slot),
            QStringLiteral("PipeWire module-pipe-source \u2022 native"),
            QStringLiteral("Ready"), pillNewStyle(),
            QString(), listFrame));
    }
    bodyLayout->addWidget(listFrame);

    // Explain block — byte-verbatim from mockup.
    auto* explain = new QFrame(this);
    explain->setStyleSheet(explainBlockStyle());
    auto* explainLayout = new QVBoxLayout(explain);
    explainLayout->setContentsMargins(12, 9, 12, 9);
    auto* explainLabel = new QLabel(explain);
    explainLabel->setTextFormat(Qt::RichText);
    explainLabel->setWordWrap(true);
    explainLabel->setText(QStringLiteral(
        "<span style='color:%1;font-weight:bold'>How it works on Linux:</span>"
        " NereusSDR dynamically loads PipeWire pipe modules at startup."
        " They unload when you quit the app. Works on both PipeWire and"
        " PulseAudio (via <code>pipewire-pulse</code> compat). In WSJT-X,"
        " pick <span style='color:%1;font-weight:bold'>\"NereusSDR VAX 1\"</span>"
        " from the audio device list.")
        .arg(Style::kTextPrimary));
    explainLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; }")
        .arg(Style::kTextSecondary));
    explainLayout->addWidget(explainLabel);
    bodyLayout->addWidget(explain);
}

void VaxFirstRunDialog::buildBodyRescanNewCables(QVBoxLayout* bodyLayout)
{
    int newCount = 0;
    for (const auto& c : m_detected) {
        if (c.isInput) {
            ++newCount;
        }
    }

    auto* intro = new QLabel(this);
    intro->setTextFormat(Qt::RichText);
    intro->setWordWrap(true);
    intro->setStyleSheet(introLabelStyle());
    intro->setText(QStringLiteral(
        "NereusSDR detected <b style='color:%1'>%2 new virtual audio"
        " cable%3</b> since your last setup. Would you like to bind"
        " them to unassigned VAX slots?")
        .arg(Style::kAccent)
        .arg(newCount)
        .arg(newCount == 1 ? QStringLiteral("") : QStringLiteral("s")));
    bodyLayout->addWidget(intro);

    auto* listFrame = new QFrame(this);
    listFrame->setObjectName(QStringLiteral("detList"));
    listFrame->setStyleSheet(detListFrameStyle());
    auto* listLayout = new QVBoxLayout(listFrame);
    listLayout->setContentsMargins(0, 4, 0, 4);
    listLayout->setSpacing(0);

    // All rescan rows are NEW by contract (Task 11b hands us only the
    // newly-appeared cables); render them as "New" pill rows starting
    // from the next unassigned slot. MainWindow in Task 11b is free to
    // compute a different slot offset — the dialog just renders what it
    // was given.
    int slot = 1;
    for (const auto& cable : m_detected) {
        if (slot > 4) {
            break;
        }
        if (!cable.isInput) {
            continue;
        }
        listLayout->addWidget(makeDetRowForCable(
            slot, cable,
            QStringLiteral("newly installed"),
            QStringLiteral("New"), pillNewStyle(),
            listFrame));
        ++slot;
    }
    bodyLayout->addWidget(listFrame);
}

QWidget* VaxFirstRunDialog::buildFooter()
{
    auto* footer = new QFrame(this);
    footer->setObjectName(QStringLiteral("dlgFtr"));
    footer->setStyleSheet(QStringLiteral(
        "QFrame#dlgFtr { background: %1; border-top: 1px solid %2; }")
        .arg(Style::kPanelBg, Style::kInsetBorder));

    auto* layout = new QHBoxLayout(footer);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(8);

    // Left-aligned hint label — content varies per scenario. The mockup
    // copy is control-surface UX, not DSP logic, so paraphrase is fine
    // where needed; the key phrases below match the mockup byte-for-byte.
    QString hintText;
    switch (m_scenario) {
        case FirstRunScenario::WindowsCablesFound:
            hintText = QStringLiteral(
                "You can re-run detection any time from Setup \u2192 Audio.");
            break;
        case FirstRunScenario::WindowsNoCables:
            hintText = QStringLiteral(
                "You can still use NereusSDR through your speakers"
                " without any cable.");
            break;
        case FirstRunScenario::MacNative:
            hintText = QStringLiteral(
                "4 native VAX channels + 1 TX input. Manage advanced"
                " settings in Setup \u2192 Audio \u2192 VAX.");
            break;
        case FirstRunScenario::LinuxNative:
            hintText = QStringLiteral(
                "4 native VAX channels + 1 TX input. Modules unload on"
                " app exit.");
            break;
        case FirstRunScenario::RescanNewCables:
            hintText = QStringLiteral(
                "NereusSDR rescans on app start and when Setup \u2192"
                " Audio is opened.");
            break;
    }
    auto* hint = new QLabel(hintText, footer);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; }")
        .arg(Style::kTextSecondary));
    layout->addWidget(hint, 1);

    // Button row — scenario-specific.
    switch (m_scenario) {
        case FirstRunScenario::WindowsCablesFound: {
            auto* customizeBtn = new QPushButton(
                QStringLiteral("Customize\u2026"), footer);
            customizeBtn->setObjectName(QString::fromUtf8(kBtnCustomize));
            customizeBtn->setStyleSheet(linkButtonStyle());
            connect(customizeBtn, &QPushButton::clicked,
                    this, &VaxFirstRunDialog::onCustomize);
            layout->addWidget(customizeBtn);

            auto* skipBtn = new QPushButton(
                QStringLiteral("Skip"), footer);
            skipBtn->setObjectName(QString::fromUtf8(kBtnSkip));
            skipBtn->setStyleSheet(neutralButtonStyle());
            connect(skipBtn, &QPushButton::clicked,
                    this, &VaxFirstRunDialog::onSkip);
            layout->addWidget(skipBtn);

            auto* applyBtn = new QPushButton(
                QStringLiteral("Apply suggested"), footer);
            applyBtn->setObjectName(QString::fromUtf8(kBtnApplySuggested));
            applyBtn->setStyleSheet(primaryButtonStyle());
            applyBtn->setDefault(true);
            connect(applyBtn, &QPushButton::clicked,
                    this, &VaxFirstRunDialog::onApplySuggested);
            layout->addWidget(applyBtn);
            break;
        }
        case FirstRunScenario::WindowsNoCables: {
            auto* whyBtn = new QPushButton(
                QStringLiteral("Why do I need this?"), footer);
            whyBtn->setObjectName(QString::fromUtf8(kBtnWhyNeeded));
            whyBtn->setStyleSheet(linkButtonStyle());
            connect(whyBtn, &QPushButton::clicked, this, [this]() {
                // Emit → MainWindow → SetupDialog::selectPage("VAX").
                emit openSetupAudioPage(QStringLiteral("VAX"));
                // reject() (not accept()) — the user hasn't completed
                // first-run; they're going to Setup for more info, same
                // semantics as Customize…
                reject();
            });
            layout->addWidget(whyBtn);

            auto* continueBtn = new QPushButton(
                QStringLiteral("Continue without VAX"), footer);
            continueBtn->setObjectName(QString::fromUtf8(kBtnSkip));
            continueBtn->setStyleSheet(neutralButtonStyle());
            connect(continueBtn, &QPushButton::clicked,
                    this, &VaxFirstRunDialog::onSkip);
            layout->addWidget(continueBtn);
            break;
        }
        case FirstRunScenario::MacNative:
        case FirstRunScenario::LinuxNative: {
            auto* gotItBtn = new QPushButton(
                QStringLiteral("Got it"), footer);
            gotItBtn->setObjectName(QString::fromUtf8(kBtnGotIt));
            gotItBtn->setStyleSheet(primaryButtonStyle());
            gotItBtn->setDefault(true);
            connect(gotItBtn, &QPushButton::clicked,
                    this, &VaxFirstRunDialog::onGotIt);
            layout->addWidget(gotItBtn);
            break;
        }
        case FirstRunScenario::RescanNewCables: {
            auto* skipBtn = new QPushButton(
                QStringLiteral("Skip for now"), footer);
            skipBtn->setObjectName(QString::fromUtf8(kBtnSkip));
            skipBtn->setStyleSheet(neutralButtonStyle());
            connect(skipBtn, &QPushButton::clicked,
                    this, &VaxFirstRunDialog::onSkip);
            layout->addWidget(skipBtn);

            // "Apply to VAX N & M" — synthesize the slot range label from
            // the suggested binding payload so the button text doesn't
            // lie when only one cable was new.
            const auto bindings = computeSuggestedBindings();
            QString applyLabel;
            if (bindings.size() == 1) {
                applyLabel = QStringLiteral("Apply to VAX %1")
                    .arg(bindings.front().first);
            } else if (bindings.size() >= 2) {
                QStringList nums;
                for (const auto& b : bindings) {
                    nums << QString::number(b.first);
                }
                applyLabel = QStringLiteral("Apply to VAX %1")
                    .arg(nums.join(QStringLiteral(" & ")));
            } else {
                applyLabel = QStringLiteral("Apply suggested");
            }

            auto* applyBtn = new QPushButton(applyLabel, footer);
            applyBtn->setObjectName(QString::fromUtf8(kBtnApplySuggested));
            applyBtn->setStyleSheet(primaryButtonStyle());
            applyBtn->setDefault(true);
            connect(applyBtn, &QPushButton::clicked,
                    this, &VaxFirstRunDialog::onApplySuggested);
            layout->addWidget(applyBtn);
            break;
        }
    }

    return footer;
}

void VaxFirstRunDialog::onApplySuggested()
{
    emit applySuggested(computeSuggestedBindings());
    accept();
}

void VaxFirstRunDialog::onSkip()
{
    // Skip / Continue without VAX is a "first-run complete, but user
    // didn't bind anything" outcome. No signal, Accepted result.
    accept();
}

void VaxFirstRunDialog::onCustomize()
{
    emit openSetupAudioPage(QStringLiteral("VAX"));
    // Customize is "dismissed without completing first-run here" —
    // SetupDialog will persist audio/FirstRunComplete on its own path.
    reject();
}

void VaxFirstRunDialog::onGotIt()
{
    // C, D: acknowledgement-only success screen. First-run complete.
    accept();
}

QString VaxFirstRunDialog::platformBadgeText() const
{
    switch (m_scenario) {
        case FirstRunScenario::WindowsCablesFound:
        case FirstRunScenario::WindowsNoCables:
        case FirstRunScenario::RescanNewCables:
            return QStringLiteral("WINDOWS");
        case FirstRunScenario::MacNative:
            return QStringLiteral("MACOS");
        case FirstRunScenario::LinuxNative:
            return QStringLiteral("LINUX");
    }
    return QStringLiteral("WINDOWS");
}

QString VaxFirstRunDialog::platformBadgeColor() const
{
    switch (m_scenario) {
        case FirstRunScenario::MacNative:
            return QString::fromUtf8(Style::kGreenText);
        case FirstRunScenario::LinuxNative:
            return QString::fromUtf8(Style::kAmberWarn);
        case FirstRunScenario::WindowsCablesFound:
        case FirstRunScenario::WindowsNoCables:
        case FirstRunScenario::RescanNewCables:
        default:
            return QString::fromUtf8(Style::kAccent);
    }
}

QString VaxFirstRunDialog::headerTitle() const
{
    if (m_scenario == FirstRunScenario::RescanNewCables) {
        return QStringLiteral("Virtual cables detected");
    }
    return QStringLiteral("Set up VAX channels");
}

} // namespace NereusSDR
