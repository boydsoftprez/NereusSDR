// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: NereusSDR-native applet, no Thetis or AetherSDR
// equivalent. See DisplayApplet.h for the full design note (3D
// Stacked-Trace Spectrum Plan Task 22).
//
// NereusSDR - DisplayApplet implementation.
//
// Modification history (NereusSDR)
//   Created 2026-09-20 by J.J. Boyd / KG4VCF, 3D Stacked-Trace Spectrum
//     Plan Task 22. AI tooling: Claude Code.

#include "DisplayApplet.h"

#include "gui/ComboStyle.h"
#include "gui/SpectrumWidget.h"
#include "gui/StyleConstants.h"
#include "gui/widgets/GuardedComboBox.h"
#include "gui/widgets/GuardedSlider.h"
#include "models/DisplaySettingsModel.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

namespace NereusSDR {

DisplayApplet::DisplayApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();

    if (m_model) {
        // Follows the active panadapter (design doc decision 6). MainWindow
        // constructs this applet from populateDefaultMeter(), which runs
        // before RadioModel::setSpectrumWidget() is called later in the
        // MainWindow constructor, so model->spectrumWidget() is commonly
        // still null here -- the initial bind is then carried entirely by
        // the signal below, fired the first time MainWindow pushes a
        // widget.
        connect(m_model, &RadioModel::spectrumWidgetChanged, this,
                [this](SpectrumWidget* w) {
            bindTo(w ? w->displaySettings() : nullptr);
        });
        if (SpectrumWidget* w = m_model->spectrumWidget()) {
            bindTo(w->displaySettings());
        }
    }
}

void DisplayApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    // Do NOT add appletTitleBar() here -- AppletPanelWidget::wrapWithTitleBar
    // already prepends a host-side title bar from appletTitle(). Adding our
    // own here results in a double header. Same fix in RadeApplet /
    // DiversityApplet / PureSignalApplet.

    auto* body = new QWidget(this);
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(4);

    // Section-title label: bold, accent-colored, matching the popup's own
    // "Waterfall" / "Spectrum" / "3D VIEW" headers (SpectrumOverlayMenu.cpp)
    // in spirit -- Style::kAccent (#00b4d8) is the same colour that popup's
    // local stylesheet hardcodes for the same purpose.
    auto addSectionTitle = [this, vbox](const QString& text) {
        auto* lbl = new QLabel(text, this);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; font-weight: bold; }"
        ).arg(Style::kAccent));
        vbox->addWidget(lbl);
    };

    // =====================================================================
    // Waterfall -- label/range/tooltip text copied verbatim from the popup
    // (SpectrumOverlayMenu.cpp's "Waterfall" section).
    // =====================================================================
    addSectionTitle(QStringLiteral("Waterfall"));

    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);
        auto* lbl = new QLabel(QStringLiteral("Color Scheme"), this);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextSecondary));
        lbl->setFixedWidth(62);
        m_colorSchemeCombo = new GuardedComboBox(this);
        m_colorSchemeCombo->addItems({
            QStringLiteral("Default"),
            QStringLiteral("Enhanced"),
            QStringLiteral("Spectran"),
            QStringLiteral("Black & White")
        });
        applyComboStyle(m_colorSchemeCombo);
        row->addWidget(lbl);
        row->addWidget(m_colorSchemeCombo, 1);
        vbox->addLayout(row);
    }

    m_colorGainSlider = new GuardedSlider(Qt::Horizontal, this);
    m_colorGainSlider->setRange(0, 100);
    m_colorGainSlider->setToolTip(QStringLiteral(
        "Waterfall color intensity — higher values increase contrast"));
    m_colorGainValue = insetValue(QStringLiteral("0"));
    vbox->addLayout(sliderRow(QStringLiteral("Color Gain"),
                               m_colorGainSlider, m_colorGainValue));

    m_blackLevelSlider = new GuardedSlider(Qt::Horizontal, this);
    m_blackLevelSlider->setRange(0, 125);
    m_blackLevelSlider->setToolTip(QStringLiteral(
        "Noise floor suppression — higher hides more noise"));
    m_blackLevelValue = insetValue(QStringLiteral("0"));
    vbox->addLayout(sliderRow(QStringLiteral("Black Level"),
                               m_blackLevelSlider, m_blackLevelValue));

    vbox->addWidget(divider());

    // =====================================================================
    // Spectrum -- Ref Level and Dyn Range copy the popup's "Display Range"
    // section verbatim; Fill Alpha and Fill trace copy the popup's own
    // "Spectrum" section verbatim; Split has no popup counterpart (model-
    // only field, spectrumFrac) and its tooltip is NereusSDR-original.
    // =====================================================================
    addSectionTitle(QStringLiteral("Spectrum"));

    m_refLevelSlider = new GuardedSlider(Qt::Horizontal, this);
    m_refLevelSlider->setRange(-160, 20);
    m_refLevelSlider->setToolTip(QStringLiteral(
        "Maximum signal level shown at top of display (dBm)"));
    m_refLevelValue = insetValue(QStringLiteral("0 dBm"));
    vbox->addLayout(sliderRow(QStringLiteral("Ref Level"),
                               m_refLevelSlider, m_refLevelValue));

    m_dynRangeSlider = new GuardedSlider(Qt::Horizontal, this);
    m_dynRangeSlider->setRange(20, 160);
    m_dynRangeSlider->setToolTip(QStringLiteral(
        "Visible dB range from ref level to bottom of display"));
    m_dynRangeValue = insetValue(QStringLiteral("0 dB"));
    vbox->addLayout(sliderRow(QStringLiteral("Dyn Range"),
                               m_dynRangeSlider, m_dynRangeValue));

    m_fillAlphaSlider = new GuardedSlider(Qt::Horizontal, this);
    m_fillAlphaSlider->setRange(0, 100);
    m_fillAlphaSlider->setToolTip(QStringLiteral(
        "Transparency of the spectrum fill area"));
    m_fillAlphaValue = insetValue(QStringLiteral("0%"));
    vbox->addLayout(sliderRow(QStringLiteral("Fill Alpha"),
                               m_fillAlphaSlider, m_fillAlphaValue));

    m_fillTraceCheck = new QCheckBox(QStringLiteral("Fill spectrum trace"), this);
    m_fillTraceCheck->setToolTip(QStringLiteral(
        "Fill the area below the spectrum trace"));
    vbox->addWidget(m_fillTraceCheck);

    m_splitSlider = new GuardedSlider(Qt::Horizontal, this);
    m_splitSlider->setRange(10, 90);
    m_splitSlider->setToolTip(QStringLiteral(
        "How much of the panadapter's height the spectrum trace fills "
        "before the waterfall starts"));
    m_splitValue = insetValue(QStringLiteral("0%"));
    vbox->addLayout(sliderRow(QStringLiteral("Split"),
                               m_splitSlider, m_splitValue));

    vbox->addWidget(divider());

    // =====================================================================
    // 3D VIEW -- label/range/tooltip text for the seven popup-sourced
    // controls copied verbatim from SpectrumOverlayMenu.cpp's "3D VIEW"
    // section (including 3D Speed, added there by Task 24). Reset 3D has
    // no popup counterpart; its button text and tooltip match
    // Display3DSetupPage's own Reset 3D button (Task 21) for wording
    // consistency across surfaces (design doc section 6.1).
    // =====================================================================
    addSectionTitle(QStringLiteral("3D VIEW"));

    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);
        auto* lbl = new QLabel(QStringLiteral("Spectrum:"), this);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; }").arg(Style::kTextSecondary));
        lbl->setFixedWidth(62);
        m_spectrumModeCombo = new GuardedComboBox(this);
        m_spectrumModeCombo->addItem(QStringLiteral("2D Waterfall"));
        m_spectrumModeCombo->addItem(QStringLiteral("3D Stacked Trace"));
        m_spectrumModeCombo->setToolTip(QStringLiteral(
            "2D: FFT trace + waterfall.\n"
            "3D: perspective stacked-trace spectrum stream."));
        applyComboStyle(m_spectrumModeCombo);
        row->addWidget(lbl);
        row->addWidget(m_spectrumModeCombo, 1);
        vbox->addLayout(row);
    }

    // 3D Floor carries no tooltip: SpectrumOverlayMenu.cpp never calls
    // setToolTip() on m_dssFloorSlider (verified by reading the file), so
    // there is no popup text to copy and none is invented here.
    m_floorSlider = new GuardedSlider(Qt::Horizontal, this);
    m_floorSlider->setRange(0, 24);
    m_floorValue = insetValue(QStringLiteral("0"));
    vbox->addLayout(sliderRow(QStringLiteral("3D Floor:"),
                               m_floorSlider, m_floorValue));

    m_gainSlider = new GuardedSlider(Qt::Horizontal, this);
    m_gainSlider->setRange(0, 100);
    m_gainSlider->setToolTip(QStringLiteral(
        "3D surface colour gain: how far down the signal range the colormap "
        "reaches.\nHigher = colour down toward the noise floor; lower = "
        "colour only on the strongest signals."));
    m_gainValue = insetValue(QStringLiteral("0"));
    vbox->addLayout(sliderRow(QStringLiteral("3D Gain:"),
                               m_gainSlider, m_gainValue));

    m_spanSlider = new GuardedSlider(Qt::Horizontal, this);
    m_spanSlider->setRange(0, 100);
    m_spanSlider->setToolTip(QStringLiteral(
        "3D surface width: how far the nearest traces overhang the plot "
        "edges,\nusing spectrum the radio sends from outside the "
        "panadapter.\nHigher = the empty wedges beside the surface close "
        "from the front;\n0 = the classic narrowing trapezoid. Limited "
        "by how much\noffscreen spectrum the source actually provides."));
    m_spanValue = insetValue(QStringLiteral("0"));
    vbox->addLayout(sliderRow(QStringLiteral("3D Span:"),
                               m_spanSlider, m_spanValue));

    m_angleSlider = new GuardedSlider(Qt::Horizontal, this);
    m_angleSlider->setRange(0, 100);
    m_angleSlider->setToolTip(QStringLiteral(
        "Viewing angle for the 3D surface: low looks along the traces "
        "edge-on, high looks down on them.\n"
        "50 is the classic fixed angle."));
    m_angleValue = insetValue(QStringLiteral("0"));
    vbox->addLayout(sliderRow(QStringLiteral("3D Angle:"),
                               m_angleSlider, m_angleValue));

    m_speedSlider = new GuardedSlider(Qt::Horizontal, this);
    m_speedSlider->setRange(0, 10);
    m_speedSlider->setToolTip(QStringLiteral(
        "How many waterfall rows each 3D row covers. Match keeps the 3D "
        "history the same length in time as the waterfall. 1:1 pushes "
        "every row, the fastest look. Higher values fold more rows into "
        "each 3D row, keeping peaks, so the surface recedes more slowly."));
    m_speedValueLabel = insetValue(QStringLiteral("Match"));
    vbox->addLayout(sliderRow(QStringLiteral("3D Speed:"),
                               m_speedSlider, m_speedValueLabel));

    m_sliceShadowCheck = new QCheckBox(QStringLiteral("3D Slice Shadow"), this);
    m_sliceShadowCheck->setToolTip(QStringLiteral(
        "Darken each slice's passband onto the 3D surface so it leans back\n"
        "with the perspective, instead of drawing flat on top of it."));
    vbox->addWidget(m_sliceShadowCheck);

    m_reset3dButton = styledButton(QStringLiteral("Reset 3D to defaults"));
    m_reset3dButton->setCheckable(false);
    m_reset3dButton->setToolTip(QStringLiteral(
        "Restore Spectrum render mode, 3D Floor, 3D Gain, 3D Span, 3D Angle, "
        "3D Speed and 3D Slice Shadow to their ship defaults (2D Waterfall / "
        "6 dB / 70% / 100% / 50% / Match / off)."));
    vbox->addWidget(m_reset3dButton);

    // Reset 3D reads m_bound at click time rather than capturing a model
    // pointer, so -- unlike the fifteen per-field connections bindTo()
    // wires against m_bindContext -- it stays correct across a rebind
    // without needing to be rewired: wiring it once here is sufficient.
    connect(m_reset3dButton, &QPushButton::clicked, this, [this]() {
        if (!m_bound) { return; }
        m_bound->setSpectrumRenderMode(0);
        m_bound->setDssFloorDepth(6);
        m_bound->setDssGain(70);
        m_bound->setDssRowSpan(100);
        m_bound->setDssAngle(50);
        m_bound->setDssRowDivider(0);
        m_bound->setThreeDSliceDepth(false);
    });

    vbox->addStretch();
    root->addWidget(body);
}

void DisplayApplet::bindTo(DisplaySettingsModel* m)
{
    // Drop every connection made against the previous bind in one shot:
    // deleting the context object disconnects any connect() that used it
    // as the receiver/context argument, which is every reflect AND every
    // push connection wired below. A stale w1 connection cannot survive
    // into the next bind.
    delete m_bindContext;
    m_bindContext = new QObject(this);
    m_bound = m;

    if (!m_bound) {
        return;
    }

    DisplaySettingsModel* bound = m_bound;

    // ---- Waterfall: model -> control ----
    connect(bound, &DisplaySettingsModel::wfColorSchemeChanged, m_bindContext,
            [this](int v) {
        QSignalBlocker block(m_colorSchemeCombo);
        m_colorSchemeCombo->setCurrentIndex(
            qBound(0, v, m_colorSchemeCombo->count() - 1));
    });
    connect(bound, &DisplaySettingsModel::wfColorGainChanged, m_bindContext,
            [this](int v) {
        QSignalBlocker block(m_colorGainSlider);
        m_colorGainSlider->setValue(v);
        m_colorGainValue->setText(QString::number(v));
    });
    connect(bound, &DisplaySettingsModel::wfBlackLevelChanged, m_bindContext,
            [this](int v) {
        QSignalBlocker block(m_blackLevelSlider);
        m_blackLevelSlider->setValue(v);
        m_blackLevelValue->setText(QString::number(v));
    });

    // ---- Spectrum: model -> control ----
    connect(bound, &DisplaySettingsModel::refLevelChanged, m_bindContext,
            [this](float dBm) {
        QSignalBlocker block(m_refLevelSlider);
        const int v = static_cast<int>(dBm);
        m_refLevelSlider->setValue(v);
        m_refLevelValue->setText(QStringLiteral("%1 dBm").arg(v));
    });
    connect(bound, &DisplaySettingsModel::dynamicRangeChanged, m_bindContext,
            [this](float dB) {
        QSignalBlocker block(m_dynRangeSlider);
        const int v = static_cast<int>(dB);
        m_dynRangeSlider->setValue(v);
        m_dynRangeValue->setText(QStringLiteral("%1 dB").arg(v));
    });
    connect(bound, &DisplaySettingsModel::fillAlphaChanged, m_bindContext,
            [this](float alpha) {
        QSignalBlocker block(m_fillAlphaSlider);
        const int v = static_cast<int>(alpha * 100.0f);
        m_fillAlphaSlider->setValue(v);
        m_fillAlphaValue->setText(QStringLiteral("%1%").arg(v));
    });
    connect(bound, &DisplaySettingsModel::panFillChanged, m_bindContext,
            [this](bool on) {
        QSignalBlocker block(m_fillTraceCheck);
        m_fillTraceCheck->setChecked(on);
    });
    connect(bound, &DisplaySettingsModel::spectrumFracChanged, m_bindContext,
            [this](float frac) {
        QSignalBlocker block(m_splitSlider);
        const int v = static_cast<int>(frac * 100.0f);
        m_splitSlider->setValue(v);
        m_splitValue->setText(QStringLiteral("%1%").arg(v));
    });

    // ---- 3D VIEW: model -> control ----
    connect(bound, &DisplaySettingsModel::spectrumRenderModeChanged, m_bindContext,
            [this](int v) {
        QSignalBlocker block(m_spectrumModeCombo);
        m_spectrumModeCombo->setCurrentIndex(
            qBound(0, v, m_spectrumModeCombo->count() - 1));
    });
    connect(bound, &DisplaySettingsModel::dssFloorDepthChanged, m_bindContext,
            [this](int v) {
        QSignalBlocker block(m_floorSlider);
        m_floorSlider->setValue(v);
        m_floorValue->setText(QString::number(v));
    });
    connect(bound, &DisplaySettingsModel::dssGainChanged, m_bindContext,
            [this](int v) {
        QSignalBlocker block(m_gainSlider);
        m_gainSlider->setValue(v);
        m_gainValue->setText(QString::number(v));
    });
    connect(bound, &DisplaySettingsModel::dssRowSpanChanged, m_bindContext,
            [this](int v) {
        QSignalBlocker block(m_spanSlider);
        m_spanSlider->setValue(v);
        m_spanValue->setText(QString::number(v));
    });
    connect(bound, &DisplaySettingsModel::dssAngleChanged, m_bindContext,
            [this](int v) {
        QSignalBlocker block(m_angleSlider);
        m_angleSlider->setValue(v);
        m_angleValue->setText(QString::number(v));
    });
    connect(bound, &DisplaySettingsModel::dssRowDividerChanged, m_bindContext,
            [this](int v) {
        QSignalBlocker block(m_speedSlider);
        m_speedSlider->setValue(v);
        m_speedValueLabel->setText(v == 0 ? QStringLiteral("Match")
                                           : QStringLiteral("1:%1").arg(v));
    });
    connect(bound, &DisplaySettingsModel::threeDSliceDepthChanged, m_bindContext,
            [this](bool on) {
        QSignalBlocker block(m_sliceShadowCheck);
        m_sliceShadowCheck->setChecked(on);
    });

    // ---- Waterfall: control -> model ----
    connect(m_colorSchemeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            m_bindContext, [bound](int idx) { bound->setWfColorScheme(idx); });
    connect(m_colorGainSlider, &QSlider::valueChanged,
            m_bindContext, [bound](int v) { bound->setWfColorGain(v); });
    connect(m_blackLevelSlider, &QSlider::valueChanged,
            m_bindContext, [bound](int v) { bound->setWfBlackLevel(v); });

    // ---- Spectrum: control -> model ----
    connect(m_refLevelSlider, &QSlider::valueChanged,
            m_bindContext, [bound](int v) {
        bound->setRefLevel(static_cast<float>(v));
    });
    connect(m_dynRangeSlider, &QSlider::valueChanged,
            m_bindContext, [bound](int v) {
        bound->setDynamicRange(static_cast<float>(v));
    });
    connect(m_fillAlphaSlider, &QSlider::valueChanged,
            m_bindContext, [bound](int v) {
        bound->setFillAlpha(static_cast<float>(v) / 100.0f);
    });
    connect(m_fillTraceCheck, &QCheckBox::toggled,
            m_bindContext, [bound](bool on) { bound->setPanFill(on); });
    connect(m_splitSlider, &QSlider::valueChanged,
            m_bindContext, [bound](int v) {
        bound->setSpectrumFrac(static_cast<float>(v) / 100.0f);
    });

    // ---- 3D VIEW: control -> model ----
    connect(m_spectrumModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            m_bindContext, [bound](int idx) { bound->setSpectrumRenderMode(idx); });
    connect(m_floorSlider, &QSlider::valueChanged,
            m_bindContext, [bound](int v) { bound->setDssFloorDepth(v); });
    connect(m_gainSlider, &QSlider::valueChanged,
            m_bindContext, [bound](int v) { bound->setDssGain(v); });
    connect(m_spanSlider, &QSlider::valueChanged,
            m_bindContext, [bound](int v) { bound->setDssRowSpan(v); });
    connect(m_angleSlider, &QSlider::valueChanged,
            m_bindContext, [bound](int v) { bound->setDssAngle(v); });
    connect(m_speedSlider, &QSlider::valueChanged,
            m_bindContext, [bound](int v) { bound->setDssRowDivider(v); });
    connect(m_sliceShadowCheck, &QCheckBox::toggled,
            m_bindContext, [bound](bool on) { bound->setThreeDSliceDepth(on); });

    syncFromModel();
}

void DisplayApplet::syncFromModel()
{
    if (!m_bound) {
        return;
    }

    {
        QSignalBlocker block(m_colorSchemeCombo);
        m_colorSchemeCombo->setCurrentIndex(
            qBound(0, m_bound->wfColorScheme(), m_colorSchemeCombo->count() - 1));
    }
    {
        QSignalBlocker block(m_colorGainSlider);
        m_colorGainSlider->setValue(m_bound->wfColorGain());
        m_colorGainValue->setText(QString::number(m_bound->wfColorGain()));
    }
    {
        QSignalBlocker block(m_blackLevelSlider);
        m_blackLevelSlider->setValue(m_bound->wfBlackLevel());
        m_blackLevelValue->setText(QString::number(m_bound->wfBlackLevel()));
    }
    {
        QSignalBlocker block(m_refLevelSlider);
        const int v = static_cast<int>(m_bound->refLevel());
        m_refLevelSlider->setValue(v);
        m_refLevelValue->setText(QStringLiteral("%1 dBm").arg(v));
    }
    {
        QSignalBlocker block(m_dynRangeSlider);
        const int v = static_cast<int>(m_bound->dynamicRange());
        m_dynRangeSlider->setValue(v);
        m_dynRangeValue->setText(QStringLiteral("%1 dB").arg(v));
    }
    {
        QSignalBlocker block(m_fillAlphaSlider);
        const int v = static_cast<int>(m_bound->fillAlpha() * 100.0f);
        m_fillAlphaSlider->setValue(v);
        m_fillAlphaValue->setText(QStringLiteral("%1%").arg(v));
    }
    {
        QSignalBlocker block(m_fillTraceCheck);
        m_fillTraceCheck->setChecked(m_bound->panFill());
    }
    {
        QSignalBlocker block(m_splitSlider);
        const int v = static_cast<int>(m_bound->spectrumFrac() * 100.0f);
        m_splitSlider->setValue(v);
        m_splitValue->setText(QStringLiteral("%1%").arg(v));
    }
    {
        QSignalBlocker block(m_spectrumModeCombo);
        m_spectrumModeCombo->setCurrentIndex(
            qBound(0, m_bound->spectrumRenderMode(), m_spectrumModeCombo->count() - 1));
    }
    {
        QSignalBlocker block(m_floorSlider);
        m_floorSlider->setValue(m_bound->dssFloorDepth());
        m_floorValue->setText(QString::number(m_bound->dssFloorDepth()));
    }
    {
        QSignalBlocker block(m_gainSlider);
        m_gainSlider->setValue(m_bound->dssGain());
        m_gainValue->setText(QString::number(m_bound->dssGain()));
    }
    {
        QSignalBlocker block(m_spanSlider);
        m_spanSlider->setValue(m_bound->dssRowSpan());
        m_spanValue->setText(QString::number(m_bound->dssRowSpan()));
    }
    {
        QSignalBlocker block(m_angleSlider);
        m_angleSlider->setValue(m_bound->dssAngle());
        m_angleValue->setText(QString::number(m_bound->dssAngle()));
    }
    {
        QSignalBlocker block(m_speedSlider);
        const int v = m_bound->dssRowDivider();
        m_speedSlider->setValue(v);
        m_speedValueLabel->setText(v == 0 ? QStringLiteral("Match")
                                           : QStringLiteral("1:%1").arg(v));
    }
    {
        QSignalBlocker block(m_sliceShadowCheck);
        m_sliceShadowCheck->setChecked(m_bound->threeDSliceDepth());
    }
}

} // namespace NereusSDR
