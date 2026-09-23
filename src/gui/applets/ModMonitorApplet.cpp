// =================================================================
// src/gui/applets/ModMonitorApplet.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original file.  See header.
// =================================================================
#include "gui/applets/ModMonitorApplet.h"

#include "core/AppSettings.h"
#include "gui/HGauge.h"
#include "gui/VintageModMeterWidget.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {
constexpr auto kKeySource   = "ModMon/Source";
constexpr auto kKeyPosFlash = "ModMon/PosFlashPct";
constexpr auto kKeyNegFlash = "ModMon/NegFlashPct";
constexpr auto kKeyFbStream = "ModMon/FbStream";
constexpr auto kKeyVintage  = "ModMon/VintageMeters";
constexpr int  kRefreshMs   = 33;
}

// ---------------------------------------------------------------------------
// ModScopeWidget
// ---------------------------------------------------------------------------
ModScopeWidget::ModScopeWidget(QWidget* parent) : QWidget(parent)
{
    setMinimumHeight(60);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ModScopeWidget::setTrace(std::vector<float> pct)
{
    m_trace = std::move(pct);
    update();
}

void ModScopeWidget::setFlashThresholds(double posPct, double negPct)
{
    m_posFlash = posPct;
    m_negFlash = negPct;
    update();
}

void ModScopeWidget::setVintage(bool on)
{
    if (m_vintage != on) {
        m_vintage = on;
        update();
    }
}

void ModScopeWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);

    if (m_vintage) {
        // Broadcast-monitor look: dark green screen, orange graticule,
        // envelope drawn mirrored about the centre line and filled.
        p.fillRect(r, QColor(0x10, 0x30, 0x18));
        p.setPen(QPen(QColor(0xc0, 0x70, 0x30, 170), 1.0));
        const int cols = 10, rows = 6;
        for (int i = 1; i < cols; ++i) {
            const double x = r.left() + r.width() * i / cols;
            p.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
        }
        for (int j = 1; j < rows; ++j) {
            const double y = r.top() + r.height() * j / rows;
            p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
        }
        p.setPen(QPen(QColor(0x90, 0x90, 0xa0), 1.0));
        p.drawRect(r);
        if (m_trace.size() >= 2) {
            // Envelope amplitude relative to carrier: (100 + pct) / 100,
            // full scale at 2.6 (= +160 %).
            const double mid = r.center().y();
            const double half = r.height() / 2.0 * 0.92;
            const double dx = r.width() / static_cast<double>(m_trace.size() - 1);
            QPainterPath top, bottom;
            for (std::size_t i = 0; i < m_trace.size(); ++i) {
                const double amp = std::clamp((100.0 + m_trace[i]) / 260.0, 0.0, 1.0);
                const double x = r.left() + dx * static_cast<double>(i);
                if (i == 0) { top.moveTo(x, mid - amp * half); bottom.moveTo(x, mid + amp * half); }
                else        { top.lineTo(x, mid - amp * half); bottom.lineTo(x, mid + amp * half); }
            }
            QPainterPath fill = top;
            for (std::size_t i = m_trace.size(); i-- > 0;) {
                const double amp = std::clamp((100.0 + m_trace[i]) / 260.0, 0.0, 1.0);
                fill.lineTo(r.left() + dx * static_cast<double>(i), mid + amp * half);
            }
            fill.closeSubpath();
            p.fillPath(fill, QColor(0x8c, 0xe0, 0x50));
            p.setPen(QPen(QColor(0x50, 0x90, 0x30), 1.0));
            p.drawPath(top);
            p.drawPath(bottom);
        }
        return;
    }
    p.fillRect(r, QColor(Style::kInsetBg));
    p.setPen(QColor(Style::kInsetBorder));
    p.drawRect(r);

    // Vertical scale: -100 % (bottom) .. +160 % (top).
    constexpr double kMin = -100.0, kMax = 160.0;
    auto yFor = [&](double pct) {
        const double t = (std::clamp(pct, kMin, kMax) - kMin) / (kMax - kMin);
        return r.bottom() - t * r.height();
    };

    // Grid: 0 (carrier), +100, -100 (solid dim), flashers (dashed red).
    QPen grid(QColor(Style::kBorderSubtle), 1.0);
    p.setPen(grid);
    p.drawLine(QPointF(r.left(), yFor(0.0)),    QPointF(r.right(), yFor(0.0)));
    p.drawLine(QPointF(r.left(), yFor(100.0)),  QPointF(r.right(), yFor(100.0)));
    p.drawLine(QPointF(r.left(), yFor(-100.0)), QPointF(r.right(), yFor(-100.0)));
    QPen flash(QColor(Style::kRedBorder), 1.0, Qt::DashLine);
    p.setPen(flash);
    p.drawLine(QPointF(r.left(), yFor(m_posFlash)),  QPointF(r.right(), yFor(m_posFlash)));
    p.drawLine(QPointF(r.left(), yFor(-m_negFlash)), QPointF(r.right(), yFor(-m_negFlash)));

    QFont f = font();
    f.setPointSizeF(std::max(6.0, f.pointSizeF() - 2.0));
    p.setFont(f);
    p.setPen(QColor(Style::kTextTertiary));
    p.drawText(QPointF(r.left() + 3, yFor(100.0) - 2),  QStringLiteral("+100"));
    p.drawText(QPointF(r.left() + 3, yFor(0.0) - 2),    QStringLiteral("0"));
    p.drawText(QPointF(r.left() + 3, yFor(-100.0) - 2), QStringLiteral("-100"));

    if (m_trace.size() < 2) {
        return;
    }
    // Trace.  Map the whole buffer across the width; positive excursions
    // in green, negative in amber, like a two-colour envelope display.
    const double dx = r.width() / static_cast<double>(m_trace.size() - 1);
    QPainterPath path;
    path.moveTo(r.left(), yFor(m_trace[0]));
    for (std::size_t i = 1; i < m_trace.size(); ++i) {
        path.lineTo(r.left() + dx * static_cast<double>(i), yFor(m_trace[i]));
    }
    // Fill between trace and carrier line for readability.
    QPainterPath fill = path;
    fill.lineTo(r.right(), yFor(0.0));
    fill.lineTo(r.left(),  yFor(0.0));
    fill.closeSubpath();
    QColor fillCol(Style::kGreenText);
    fillCol.setAlpha(40);
    p.fillPath(fill, fillCol);
    p.setPen(QPen(QColor(Style::kGreenText), 1.2));
    p.drawPath(path);
}

// ---------------------------------------------------------------------------
// ModMonitorApplet
// ---------------------------------------------------------------------------
ModMonitorApplet::ModMonitorApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();
    loadSettings();
    m_timer.setInterval(kRefreshMs);
    connect(&m_timer, &QTimer::timeout, this, &ModMonitorApplet::tick);
}

void ModMonitorApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    // Title bar is added by AppletPanelWidget::wrapWithTitleBar.

    auto* body = new QWidget(this);
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(4, 2, 4, 4);
    vbox->setSpacing(3);

    // ── Row 1: source + reset ─────────────────────────────────────────────
    auto* srcRow = new QHBoxLayout();
    srcRow->setSpacing(3);
    m_srcTxBtn = blueToggle(QStringLiteral("TX I/Q"), 58);
    m_srcTxBtn->setToolTip(QStringLiteral(
        "Measure the modulation NereusSDR generates (the I/Q sent to the radio)."));
    m_srcFbBtn = blueToggle(QStringLiteral("PA FB"), 52);
    m_srcFbBtn->setToolTip(QStringLiteral(
        "Measure the PA output via the PureSignal feedback receiver.  Needs "
        "PureSignal feedback running on the connected radio."));
    m_fbStreamSpin = new QSpinBox(body);
    m_fbStreamSpin->setRange(0, 4);
    m_fbStreamSpin->setPrefix(QStringLiteral("rx"));
    m_fbStreamSpin->setToolTip(QStringLiteral(
        "Receiver stream carrying the PureSignal feedback.  Hermes Lite 2: rx1."));
    m_fbStreamSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
    m_fbStreamSpin->setFixedWidth(46);
    m_resetBtn = styledButton(QStringLiteral("RESET"), 52);
    m_resetBtn->setToolTip(QStringLiteral("Clear peak-hold and flashers."));
    m_vuBtn = amberToggle(QStringLiteral("VU"), 34);
    m_vuBtn->setToolTip(QStringLiteral(
        "Vintage illuminated meters instead of bar graphs."));
    srcRow->addWidget(m_srcTxBtn);
    srcRow->addWidget(m_srcFbBtn);
    srcRow->addWidget(m_fbStreamSpin);
    srcRow->addStretch(1);
    srcRow->addWidget(m_vuBtn);
    srcRow->addWidget(m_resetBtn);
    connect(m_vuBtn, &QPushButton::toggled, this, &ModMonitorApplet::setVintageMeters);
    vbox->addLayout(srcRow);

    connect(m_srcTxBtn, &QPushButton::clicked, this, [this]() { setSource(Source::TxIq); });
    connect(m_srcFbBtn, &QPushButton::clicked, this, [this]() { setSource(Source::PaFeedback); });
    connect(m_resetBtn, &QPushButton::clicked, this, &ModMonitorApplet::resetPeaks);
    connect(m_fbStreamSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        AppSettings::instance().setValue(QString::fromLatin1(kKeyFbStream), QString::number(v));
        if (m_model) {
            m_model->setAmModFeedbackStream(v);
        }
    });

    // ── Row 2/3: peak meters ───────────────────────────────────────────────
    m_posGauge = new HGauge(body);
    m_posGauge->setRange(0.0, 160.0);
    m_posGauge->setYellowStart(100.0);
    m_posGauge->setRedStart(125.0);
    m_posGauge->setTitle(QStringLiteral("+PK"));
    m_posGauge->setUnit(QStringLiteral("%"));
    m_posGauge->setTickLabels({QStringLiteral("0"), QStringLiteral("50"),
                               QStringLiteral("100"), QStringLiteral("125"),
                               QStringLiteral("160")});
    m_negGauge = new HGauge(body);
    m_negGauge->setRange(0.0, 100.0);
    m_negGauge->setYellowStart(90.0);
    m_negGauge->setRedStart(98.0);
    m_negGauge->setTitle(QStringLiteral("-PK"));
    m_negGauge->setUnit(QStringLiteral("%"));
    m_negGauge->setTickLabels({QStringLiteral("0"), QStringLiteral("25"),
                               QStringLiteral("50"), QStringLiteral("75"),
                               QStringLiteral("100")});
    // Page 0: bar graphs.  Page 1: vintage meter pair with asymmetry bar.
    m_meterStack = new QStackedWidget(body);
    auto* barsPage = new QWidget(m_meterStack);
    auto* barsLay = new QVBoxLayout(barsPage);
    barsLay->setContentsMargins(0, 0, 0, 0);
    barsLay->setSpacing(3);
    barsLay->addWidget(m_posGauge);
    barsLay->addWidget(m_negGauge);
    m_meterStack->addWidget(barsPage);

    auto* vuPage = new QWidget(m_meterStack);
    auto* vuLay = new QHBoxLayout(vuPage);
    vuLay->setContentsMargins(0, 0, 0, 0);
    vuLay->setSpacing(3);
    m_negMeter = new VintageModMeterWidget(vuPage);
    m_negMeter->setCaption(QStringLiteral("NEGATIVE PEAKS"));
    m_posMeter = new VintageModMeterWidget(vuPage);
    m_posMeter->setCaption(QStringLiteral("POSITIVE PEAKS"));
    m_asymBar = new AsymmetryBarWidget(vuPage);
    vuLay->addWidget(m_negMeter, 1);
    vuLay->addWidget(m_asymBar);
    vuLay->addWidget(m_posMeter, 1);
    vuPage->setMinimumHeight(132);
    m_meterStack->addWidget(vuPage);
    vbox->addWidget(m_meterStack);

    // ── Row 4: readouts ───────────────────────────────────────────────────
    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(4);
    grid->setVerticalSpacing(2);
    auto* lblPos  = new QLabel(QStringLiteral("+PK"), body);
    auto* lblNeg  = new QLabel(QStringLiteral("-PK"), body);
    auto* lblAsym = new QLabel(QStringLiteral("ASYM"), body);
    auto* lblCar  = new QLabel(QStringLiteral("CARR"), body);
    for (QLabel* l : {lblPos, lblNeg, lblAsym, lblCar}) {
        l->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    }
    m_posValue     = insetValue(QStringLiteral("--"), 46);
    m_negValue     = insetValue(QStringLiteral("--"), 46);
    m_asymValue    = insetValue(QStringLiteral("--"), 46);
    m_carrierValue = insetValue(QStringLiteral("--"), 56);
    grid->addWidget(lblPos,         0, 0); grid->addWidget(m_posValue,     0, 1);
    grid->addWidget(lblNeg,         0, 2); grid->addWidget(m_negValue,     0, 3);
    grid->addWidget(lblAsym,        1, 0); grid->addWidget(m_asymValue,    1, 1);
    grid->addWidget(lblCar,         1, 2); grid->addWidget(m_carrierValue, 1, 3);
    vbox->addLayout(grid);

    // ── Row 5: flashers + carrier lamp ────────────────────────────────────
    auto* lampRow = new QHBoxLayout();
    lampRow->setSpacing(3);
    m_posFlasher = new QLabel(QStringLiteral("+PEAK"), body);
    m_negFlasher = new QLabel(QStringLiteral("-PEAK"), body);
    m_carrierLamp = new QLabel(QStringLiteral("NO CARRIER"), body);
    for (QLabel* l : {m_posFlasher, m_negFlasher, m_carrierLamp}) {
        l->setAlignment(Qt::AlignCenter);
        l->setMinimumHeight(18);
    }
    m_posFlashSpin = new QSpinBox(body);
    m_posFlashSpin->setRange(50, 160);
    m_posFlashSpin->setSuffix(QStringLiteral("%"));
    m_posFlashSpin->setToolTip(QStringLiteral("Positive peak flasher threshold."));
    m_posFlashSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
    m_posFlashSpin->setFixedWidth(54);
    m_negFlashSpin = new QSpinBox(body);
    m_negFlashSpin->setRange(50, 100);
    m_negFlashSpin->setSuffix(QStringLiteral("%"));
    m_negFlashSpin->setToolTip(QStringLiteral("Negative peak flasher threshold."));
    m_negFlashSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
    m_negFlashSpin->setFixedWidth(54);
    lampRow->addWidget(m_posFlasher, 1);
    lampRow->addWidget(m_posFlashSpin);
    lampRow->addWidget(m_negFlasher, 1);
    lampRow->addWidget(m_negFlashSpin);
    vbox->addLayout(lampRow);
    vbox->addWidget(m_carrierLamp);

    auto persistFlash = [this]() {
        AppSettings::instance().setValue(QString::fromLatin1(kKeyPosFlash),
                                         QString::number(m_posFlashSpin->value()));
        AppSettings::instance().setValue(QString::fromLatin1(kKeyNegFlash),
                                         QString::number(m_negFlashSpin->value()));
        if (m_scope) {
            m_scope->setFlashThresholds(m_posFlashSpin->value(), m_negFlashSpin->value());
        }
    };
    connect(m_posFlashSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, persistFlash);
    connect(m_negFlashSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, persistFlash);

    // ── Row 6: scope ──────────────────────────────────────────────────────
    m_scope = new ModScopeWidget(body);
    vbox->addWidget(m_scope, 1);

    root->addWidget(body);

    setLamp(m_posFlasher, QStringLiteral("+PEAK"), Style::kInsetBg, Style::kTextInactive, Style::kInsetBorder);
    setLamp(m_negFlasher, QStringLiteral("-PEAK"), Style::kInsetBg, Style::kTextInactive, Style::kInsetBorder);
    setLamp(m_carrierLamp, QStringLiteral("NO CARRIER"), Style::kInsetBg, Style::kTextInactive, Style::kInsetBorder);
}

void ModMonitorApplet::loadSettings()
{
    auto& s = AppSettings::instance();
    const int src = s.value(QString::fromLatin1(kKeySource), QStringLiteral("0")).toInt();
    const int pos = s.value(QString::fromLatin1(kKeyPosFlash), QStringLiteral("125")).toInt();
    const int neg = s.value(QString::fromLatin1(kKeyNegFlash), QStringLiteral("95")).toInt();
    const int fb  = s.value(QString::fromLatin1(kKeyFbStream), QStringLiteral("1")).toInt();
    const bool vu = s.value(QString::fromLatin1(kKeyVintage), QStringLiteral("False")).toString()
                    == QLatin1String("True");
    {
        const QSignalBlocker b1(m_posFlashSpin);
        const QSignalBlocker b2(m_negFlashSpin);
        const QSignalBlocker b3(m_fbStreamSpin);
        m_posFlashSpin->setValue(pos);
        m_negFlashSpin->setValue(neg);
        m_fbStreamSpin->setValue(fb);
    }
    m_scope->setFlashThresholds(pos, neg);
    if (m_model) {
        m_model->setAmModFeedbackStream(fb);
    }
    setSource(src == 1 ? Source::PaFeedback : Source::TxIq);
    {
        const QSignalBlocker b(m_vuBtn);
        m_vuBtn->setChecked(vu);
    }
    setVintageMeters(vu);
}

void ModMonitorApplet::setVintageMeters(bool on)
{
    m_vintage = on;
    if (m_meterStack) {
        m_meterStack->setCurrentIndex(on ? 1 : 0);
    }
    if (m_scope) {
        m_scope->setVintage(on);
    }
    AppSettings::instance().setValue(QString::fromLatin1(kKeyVintage),
                                     on ? QStringLiteral("True") : QStringLiteral("False"));
}

double ModMonitorApplet::posFlashPct() const { return m_posFlashSpin ? m_posFlashSpin->value() : 125.0; }
double ModMonitorApplet::negFlashPct() const { return m_negFlashSpin ? m_negFlashSpin->value() : 95.0; }

void ModMonitorApplet::setSource(Source src)
{
    m_source = src;
    m_srcTxBtn->setChecked(src == Source::TxIq);
    m_srcFbBtn->setChecked(src == Source::PaFeedback);
    AppSettings::instance().setValue(QString::fromLatin1(kKeySource),
                                     QString::number(static_cast<int>(src)));
    if (m_model) {
        m_model->setAmModFeedbackWanted(src == Source::PaFeedback);
    }
    resetPeaks();
}

void ModMonitorApplet::resetPeaks()
{
    if (m_model) {
        if (auto* a = m_model->amModulationAnalyzer(static_cast<int>(m_source))) {
            a->reset();
        }
    }
    m_posGauge->setPeakValue(-999.0);
    m_negGauge->setPeakValue(-999.0);
    if (m_posMeter) { m_posMeter->setPeakValue(-1.0); }
    if (m_negMeter) { m_negMeter->setPeakValue(-1.0); }
    m_posLit = m_negLit = false;
    setLamp(m_posFlasher, QStringLiteral("+PEAK"), Style::kInsetBg, Style::kTextInactive, Style::kInsetBorder);
    setLamp(m_negFlasher, QStringLiteral("-PEAK"), Style::kInsetBg, Style::kTextInactive, Style::kInsetBorder);
}

void ModMonitorApplet::syncFromModel()
{
    // Nothing model-driven to mirror beyond the live tick.
}

void ModMonitorApplet::showEvent(QShowEvent* e)
{
    AppletWidget::showEvent(e);
    m_timer.start();
}

void ModMonitorApplet::hideEvent(QHideEvent* e)
{
    m_timer.stop();
    AppletWidget::hideEvent(e);
}

void ModMonitorApplet::tick()
{
    if (!m_model) {
        return;
    }
    auto* a = m_model->amModulationAnalyzer(static_cast<int>(m_source));
    if (!a) {
        return;
    }
    applySnapshot(a->snapshot());
}

void ModMonitorApplet::setLamp(QLabel* lamp, const QString& text,
                               const char* bg, const char* fg, const char* border)
{
    lamp->setText(text);
    lamp->setStyleSheet(QStringLiteral(
        "QLabel { background:%1; color:%2; border:1px solid %3; border-radius:2px; "
        "font-weight:bold; font-size:10px; padding:1px 3px; }")
        .arg(QLatin1String(bg), QLatin1String(fg), QLatin1String(border)));
}

QString ModMonitorApplet::carrierLampTextForTest() const
{
    return m_carrierLamp ? m_carrierLamp->text() : QString();
}

void ModMonitorApplet::applySnapshot(const AmModulationAnalyzer::Snapshot& s)
{
    const bool live = s.carrierPresent;
    const double pos = live ? s.posPeakPct : 0.0;
    const double neg = live ? s.negPeakPct : 0.0;

    m_posGauge->setValue(pos);
    m_negGauge->setValue(neg);
    m_posGauge->setPeakValue(live && s.posHoldPct > 0.0 ? s.posHoldPct : -999.0);
    m_negGauge->setPeakValue(live && s.negHoldPct > 0.0 ? s.negHoldPct : -999.0);
    if (m_posMeter && m_negMeter && m_asymBar) {
        m_posMeter->setLit(live);
        m_negMeter->setLit(live);
        m_posMeter->setValue(pos);
        m_negMeter->setValue(neg);
        m_posMeter->setPeakValue(live && s.posHoldPct > 0.0 ? s.posHoldPct : -1.0);
        m_negMeter->setPeakValue(live && s.negHoldPct > 0.0 ? s.negHoldPct : -1.0);
        m_asymBar->setValue(live ? (s.posHoldPct - s.negHoldPct) : 0.0);
    }

    if (live) {
        m_posValue->setText(QStringLiteral("%1").arg(s.posHoldPct, 0, 'f', 0));
        m_negValue->setText(QStringLiteral("%1").arg(s.negHoldPct, 0, 'f', 0));
        // Asymmetry: positive-over-negative ratio in dB-free percent
        // points, the way the AMM-SD1 bar graph presents it.
        m_asymValue->setText(QStringLiteral("%1%2")
                                 .arg(s.posHoldPct - s.negHoldPct >= 0 ? QStringLiteral("+") : QString())
                                 .arg(s.posHoldPct - s.negHoldPct, 0, 'f', 0));
        m_carrierValue->setText(QStringLiteral("%1 dB").arg(s.carrierDbfs, 0, 'f', 1));
    } else {
        m_posValue->setText(QStringLiteral("--"));
        m_negValue->setText(QStringLiteral("--"));
        m_asymValue->setText(QStringLiteral("--"));
        m_carrierValue->setText(QStringLiteral("--"));
    }

    // Flashers latch until RESET or a new key-down clears the analyzer.
    if (live && s.posPeakPct >= posFlashPct() && !m_posLit) {
        m_posLit = true;
        setLamp(m_posFlasher, QStringLiteral("+PEAK"), Style::kRedBg, Style::kRedText, Style::kRedBorder);
    }
    if (live && s.negPeakPct >= negFlashPct() && !m_negLit) {
        m_negLit = true;
        setLamp(m_negFlasher, QStringLiteral("-PEAK"), Style::kRedBg, Style::kRedText, Style::kRedBorder);
    }

    if (!live) {
        setLamp(m_carrierLamp, QStringLiteral("NO CARRIER"), Style::kInsetBg, Style::kTextInactive, Style::kInsetBorder);
    } else if (s.carrierHigh) {
        setLamp(m_carrierLamp, QStringLiteral("CARRIER HIGH"), Style::kRedBg, Style::kRedText, Style::kRedBorder);
    } else if (s.carrierLow) {
        setLamp(m_carrierLamp, QStringLiteral("CARRIER LOW"), Style::kAmberBg, Style::kAmberText, Style::kAmberBorder);
    } else {
        setLamp(m_carrierLamp, QStringLiteral("CARRIER OK"), Style::kGreenBg, Style::kGreenText, Style::kGreenBorder);
    }

    m_scope->setTrace(s.scope);
}

} // namespace NereusSDR
