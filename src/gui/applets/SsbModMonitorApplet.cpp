// =================================================================
// src/gui/applets/SsbModMonitorApplet.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original file. See the header.
//
// Modification history (NereusSDR):
//   2026-11-04 — Created. Brian Sutton (W4KZO), AI-assisted via xAI Grok.
//   2026-11-05 - Corrections for review findings.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-11-14 - The vintage scope marks the OVER threshold on the envelope
//                scale. Brian Sutton (W4KZO), AI-assisted via xAI Grok.
// =================================================================
#include "gui/applets/SsbModMonitorApplet.h"

#include "core/AppSettings.h"
#include "core/session/IStationLink.h"
#include "gui/HGauge.h"
#include "gui/StyleConstants.h"
#include "gui/VintageModMeterWidget.h"
#include "gui/styles/WorkspacePreview.h"
#include "models/RadioModel.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {
constexpr auto kKeySource   = "SsbModMon/Source";
constexpr auto kKeyOverDb   = "SsbModMon/OverDb";
constexpr auto kKeyVintage  = "SsbModMon/VintageMeters";
constexpr int  kRefreshMs   = 33;
constexpr double kScopeMax  = 120.0;

QString paFeedbackToolTip()
{
    return QStringLiteral("Measure the PA output through PureSignal's feedback.");
}

// Inverse of SsbModulationAnalyzer::toDbfs. 100 percent is 0 dBFS, which is
// linear 1.0, the level the DAC clips. The lamp, this line, and the meter
// face's decibel ticks all use it.
double percentForDbfs(double dbfs)
{
    return 100.0 * std::pow(10.0, dbfs / 20.0);
}

// Share of full-scale deflection. 0 dBFS is the full swing. Past that,
// the vintage trace stays on the edge of the screen.
double envelopeFraction(double pct)
{
    return std::clamp(pct / 100.0, 0.0, 1.0);
}
}

SsbModScopeWidget::SsbModScopeWidget(QWidget* parent) : QWidget(parent)
{
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void SsbModScopeWidget::setTrace(std::vector<float> pct)
{
    m_trace = std::move(pct);
    update();
}

void SsbModScopeWidget::setOverThresholdPct(double pct)
{
    m_overPct = pct;
    update();
}

void SsbModScopeWidget::setVintage(bool on)
{
    if (m_vintage != on) {
        m_vintage = on;
        update();
    }
}

void SsbModScopeWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    if (m_vintage) {
        // Same screen as the AM monitor's VU scope: dark green, an orange
        // graticule of 10 columns by 6 rows, and the envelope mirrored
        // about the centre line. 100% of full scale is full deflection.
        // The dashed pair is the OVER threshold on that same scale.
        p.fillRect(r, QColor(0x10, 0x30, 0x18));
        p.setPen(QPen(QColor(0xc0, 0x70, 0x30, 170), 1.0));
        constexpr int kCols = 10;
        constexpr int kRows = 6;
        for (int i = 1; i < kCols; ++i) {
            const double x = r.left() + r.width() * i / kCols;
            p.drawLine(QPointF(x, r.top()), QPointF(x, r.bottom()));
        }
        for (int j = 1; j < kRows; ++j) {
            const double y = r.top() + r.height() * j / kRows;
            p.drawLine(QPointF(r.left(), y), QPointF(r.right(), y));
        }
        p.setPen(QPen(QColor(0x90, 0x90, 0xa0), 1.0));
        p.drawRect(r);
        const double mid = r.center().y();
        const double half = r.height() / 2.0 * 0.92;
        if (m_trace.size() >= 2) {
            const double dx = r.width() / static_cast<double>(m_trace.size() - 1);
            QPainterPath top;
            top.moveTo(r.left(), mid);
            for (std::size_t i = 0; i < m_trace.size(); ++i) {
                const double amp = envelopeFraction(m_trace[i]);
                const double x = r.left() + dx * static_cast<double>(i);
                if (i == 0) {
                    top.moveTo(x, mid - amp * half);
                } else {
                    top.lineTo(x, mid - amp * half);
                }
            }
            QPainterPath fill = top;
            for (std::size_t i = m_trace.size(); i-- > 0;) {
                const double amp = envelopeFraction(m_trace[i]);
                fill.lineTo(r.left() + dx * static_cast<double>(i), mid + amp * half);
            }
            fill.closeSubpath();
            p.fillPath(fill, QColor(0x8c, 0xe0, 0x50));
            p.setPen(QPen(QColor(0x50, 0x90, 0x30), 1.0));
            p.drawPath(top);
            QPainterPath bottom;
            for (std::size_t i = 0; i < m_trace.size(); ++i) {
                const double amp = envelopeFraction(m_trace[i]);
                const double x = r.left() + dx * static_cast<double>(i);
                const double y = mid + amp * half;
                if (i == 0) {
                    bottom.moveTo(x, y);
                } else {
                    bottom.lineTo(x, y);
                }
            }
            p.drawPath(bottom);
        }
        const double overAmp = envelopeFraction(m_overPct);
        p.setPen(QPen(QColor(Style::kRedBorder), 1.0, Qt::DashLine));
        p.drawLine(QPointF(r.left(), mid - overAmp * half), QPointF(r.right(), mid - overAmp * half));
        p.drawLine(QPointF(r.left(), mid + overAmp * half), QPointF(r.right(), mid + overAmp * half));
        return;
    }

    p.fillRect(r, QColor(Style::kInsetBg));
    p.setPen(QColor(Style::kInsetBorder));
    p.drawRect(r);

    auto yFor = [&](double pct) {
        const double t = (std::clamp(pct, 0.0, kScopeMax)) / kScopeMax;
        return r.bottom() - t * r.height();
    };
    p.setPen(QPen(QColor(Style::kBorderSubtle), 1.0));
    p.drawLine(QPointF(r.left(), yFor(0.0)), QPointF(r.right(), yFor(0.0)));
    p.drawLine(QPointF(r.left(), yFor(50.0)), QPointF(r.right(), yFor(50.0)));
    p.drawLine(QPointF(r.left(), yFor(100.0)), QPointF(r.right(), yFor(100.0)));
    p.setPen(QPen(QColor(Style::kRedBorder), 1.0, Qt::DashLine));
    p.drawLine(QPointF(r.left(), yFor(m_overPct)), QPointF(r.right(), yFor(m_overPct)));

    QFont f = font();
    f.setPointSizeF(std::max(6.0, f.pointSizeF() - 2.0));
    f = WorkspacePreview::readableFont(f);
    p.setFont(f);
    p.setPen(QColor(Style::kTextTertiary));
    p.drawText(QPointF(r.left() + 3, yFor(100.0) - 2), QStringLiteral("100"));
    p.drawText(QPointF(r.left() + 3, yFor(50.0) - 2), QStringLiteral("50"));
    p.drawText(QPointF(r.left() + 3, yFor(0.0) - 2), QStringLiteral("0"));

    if (m_trace.size() < 2) {
        return;
    }
    const double dx = r.width() / static_cast<double>(m_trace.size() - 1);
    QPainterPath path;
    path.moveTo(r.left(), yFor(m_trace[0]));
    for (std::size_t i = 1; i < m_trace.size(); ++i) {
        path.lineTo(r.left() + dx * static_cast<double>(i), yFor(m_trace[i]));
    }
    QPainterPath fill = path;
    fill.lineTo(r.right(), yFor(0.0));
    fill.lineTo(r.left(), yFor(0.0));
    fill.closeSubpath();
    QColor fillCol(Style::kGreenText);
    fillCol.setAlpha(40);
    p.fillPath(fill, fillCol);
    p.setPen(QPen(QColor(Style::kGreenText), 1.2));
    p.drawPath(path);
}

SsbModMonitorApplet::SsbModMonitorApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    setMinimumHeight(360);
    buildUI();
    loadSettings();
    m_timer.setInterval(kRefreshMs);
    connect(&m_timer, &QTimer::timeout, this, &SsbModMonitorApplet::tick);
    if (isRemoteWindow()) {
        connect(m_model, &RadioModel::stationLinkStateChanged,
                this, &SsbModMonitorApplet::updateRemoteAvailability);
        updateRemoteAvailability();
    }
    if (m_model) {
        connect(m_model, &RadioModel::ssbModFeedbackInUseChanged,
                this, &SsbModMonitorApplet::updatePaFeedbackAvailability);
    }
    updatePaFeedbackAvailability();
}

bool SsbModMonitorApplet::isRemoteWindow() const
{
    return m_model && m_model->role() == RadioModel::Role::Remote;
}

void SsbModMonitorApplet::updatePaFeedbackAvailability()
{
    const bool inUse = m_model && m_model->ssbModFeedbackInUse();
    // Disabled, not hidden, and a saved PA FB choice stays chosen: it reads
    // nothing until PureSignal runs.
    m_srcFbBtn->setEnabled(inUse);
    m_srcFbBtn->setToolTip(inUse ? paFeedbackToolTip() : paFeedbackOffReason());
}

QString SsbModMonitorApplet::paFeedbackOffReason()
{
    return QStringLiteral("PA FB reads PureSignal's feedback. PureSignal is off, or this radio does not have it.");
}

void SsbModMonitorApplet::syncLocalMeasuring(bool shown)
{
    if (!m_model || isRemoteWindow()) {
        return;
    }
    m_model->setSsbModTxTapEnabled(shown && m_source == Source::TxIq);
    m_model->setSsbModFeedbackWanted(shown && m_source == Source::PaFeedback);
}

QString SsbModMonitorApplet::notConnectedReason()
{
    return QStringLiteral("Connect to the Core to see the SSB modulation monitor.");
}

QString SsbModMonitorApplet::unavailableReasonForTest() const
{
    return m_unavailableLabel && !m_available ? m_unavailableLabel->text() : QString();
}

QString SsbModMonitorApplet::lampTextForTest() const
{
    return m_lamp ? m_lamp->text() : QString();
}

QString SsbModMonitorApplet::crestTextForTest() const
{
    return m_crestValue ? m_crestValue->text() : QString();
}

bool SsbModMonitorApplet::paFeedbackEnabledForTest() const
{
    return m_srcFbBtn && m_srcFbBtn->isEnabled();
}

QString SsbModMonitorApplet::paFeedbackToolTipForTest() const
{
    return m_srcFbBtn ? m_srcFbBtn->toolTip() : QString();
}

std::size_t SsbModMonitorApplet::scopePointsForTest() const
{
    return m_scope ? m_scope->tracePoints() : 0;
}

double SsbModMonitorApplet::overThresholdDbfs() const
{
    return m_overSpin ? m_overSpin->value() : -1.0;
}

void SsbModMonitorApplet::setOverThresholdDbForTest(int db)
{
    if (m_overSpin == nullptr) {
        return;
    }
    const QSignalBlocker block(m_overSpin);
    m_overSpin->setValue(db);
    if (m_scope != nullptr) {
        m_scope->setOverThresholdPct(percentForDbfs(db));
    }
}

double SsbModMonitorApplet::scopeOverPercentForTest() const
{
    return m_scope != nullptr ? m_scope->overThresholdPct() : 0.0;
}

void SsbModMonitorApplet::updateRemoteAvailability()
{
    if (!isRemoteWindow()) {
        return;
    }
    const IStationLink* link = m_model->stationLink();
    const bool ready = link != nullptr && link->stationLinkReady();
    const bool available = ready && link->txSsbModMonitorAvailable();
    const QString reason = !ready ? notConnectedReason()
                                  : IStationLink::ssbModMonitorUnavailableReason();
    m_available = available;
    m_body->setEnabled(available);
    m_body->setToolTip(available ? QString() : reason);
    m_unavailableLabel->setText(reason);
    m_unavailableLabel->setVisible(!available);
    if (!available) {
        applySnapshot(SsbModulationAnalyzer::Snapshot{});
        setLamp(m_lamp, QStringLiteral("--"), Style::kInsetBg, Style::kTextInactive, Style::kInsetBorder);
    }
    if (isVisible()) {
        watchOnCore(static_cast<int>(m_source));
    }
}

void SsbModMonitorApplet::watchOnCore(int source)
{
    if (!isRemoteWindow()) {
        return;
    }
    if (IStationLink* link = m_model->stationLink()) {
        link->setSsbModMonitorSource(source);
    }
}

void SsbModMonitorApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_unavailableLabel = new QLabel(this);
    m_unavailableLabel->setWordWrap(true);
    m_unavailableLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    m_unavailableLabel->setContentsMargins(4, 2, 4, 0);
    m_unavailableLabel->setVisible(false);
    root->addWidget(m_unavailableLabel);

    auto* body = new QWidget(this);
    m_body = body;
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(4, 2, 4, 4);
    vbox->setSpacing(3);

    auto* srcRow = new QHBoxLayout();
    srcRow->setSpacing(3);
    m_srcTxBtn = blueToggle(QStringLiteral("TX I/Q"), 58);
    m_srcTxBtn->setToolTip(QStringLiteral(
        "Measure the USB or LSB envelope NereusSDR generates "
        "(the I/Q sent to the radio)."));
    m_srcFbBtn = blueToggle(QStringLiteral("PA FB"), 52);
    m_resetBtn = styledButton(QStringLiteral("RESET"), 52);
    m_resetBtn->setToolTip(QStringLiteral("Clear peak-hold and the flasher."));
    m_vuBtn = amberToggle(QStringLiteral("VU"), 34);
    m_vuBtn->setToolTip(QStringLiteral(
        "Illuminated analog gauges, the same faces as the AM Mod Monitor."));
    srcRow->addWidget(m_srcTxBtn);
    srcRow->addWidget(m_srcFbBtn);
    srcRow->addStretch(1);
    srcRow->addWidget(m_vuBtn);
    srcRow->addWidget(m_resetBtn);
    connect(m_vuBtn, &QPushButton::toggled, this, &SsbModMonitorApplet::setVintageMeters);
    vbox->addLayout(srcRow);

    connect(m_srcTxBtn, &QPushButton::clicked, this, [this]() { setSource(Source::TxIq); });
    connect(m_srcFbBtn, &QPushButton::clicked, this, [this]() { setSource(Source::PaFeedback); });
    connect(m_resetBtn, &QPushButton::clicked, this, &SsbModMonitorApplet::resetPeaks);

    m_peakGauge = new HGauge(body);
    m_peakGauge->setRange(0.0, 120.0);
    m_peakGauge->setYellowStart(80.0);
    m_peakGauge->setRedStart(100.0);
    m_peakGauge->setTitle(QStringLiteral("PEAK"));
    m_peakGauge->setUnit(QStringLiteral("%"));
    m_peakGauge->setTickLabels({QStringLiteral("0"), QStringLiteral("50"),
                                QStringLiteral("100"), QStringLiteral("120")});
    m_avgGauge = new HGauge(body);
    m_avgGauge->setRange(0.0, 120.0);
    m_avgGauge->setYellowStart(80.0);
    m_avgGauge->setRedStart(100.0);
    m_avgGauge->setTitle(QStringLiteral("AVG"));
    m_avgGauge->setUnit(QStringLiteral("%"));
    m_avgGauge->setTickLabels({QStringLiteral("0"), QStringLiteral("50"),
                               QStringLiteral("100"), QStringLiteral("120")});
    // Page 0: the bar gauges. Page 1: the illuminated analog faces.
    m_meterStack = new QStackedWidget(body);
    auto* barsPage = new QWidget(m_meterStack);
    auto* barsLay = new QVBoxLayout(barsPage);
    barsLay->setContentsMargins(0, 0, 0, 0);
    barsLay->setSpacing(3);
    barsLay->addWidget(m_peakGauge);
    barsLay->addWidget(m_avgGauge);
    m_meterStack->addWidget(barsPage);

    auto* vuPage = new QWidget(m_meterStack);
    auto* vuLay = new QHBoxLayout(vuPage);
    vuLay->setContentsMargins(0, 0, 0, 0);
    vuLay->setSpacing(3);
    m_peakMeter = new VintageModMeterWidget(vuPage);
    m_peakMeter->setCaption(QStringLiteral("PEAK"));
    m_peakMeter->setRange(0.0, 140.0);
    m_peakMeter->setRedZoneStart(100.0);
    m_avgMeter = new VintageModMeterWidget(vuPage);
    m_avgMeter->setCaption(QStringLiteral("AVERAGE"));
    m_avgMeter->setRange(0.0, 140.0);
    m_avgMeter->setRedZoneStart(100.0);
    vuLay->addWidget(m_peakMeter, 1);
    vuLay->addWidget(m_avgMeter, 1);
    vuPage->setMinimumHeight(132);
    m_meterStack->addWidget(vuPage);
    vbox->addWidget(m_meterStack);

    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(4);
    grid->setVerticalSpacing(2);
    auto* lblPeak  = new QLabel(QStringLiteral("PEAK"), body);
    auto* lblAvg   = new QLabel(QStringLiteral("AVG"), body);
    auto* lblCrest = new QLabel(QStringLiteral("CREST"), body);
    auto* lblPct   = new QLabel(QStringLiteral("%FS"), body);
    for (QLabel* l : {lblPeak, lblAvg, lblCrest, lblPct}) {
        l->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    }
    m_peakValue  = insetValue(QStringLiteral("--"), 52);
    m_avgValue   = insetValue(QStringLiteral("--"), 52);
    m_crestValue = insetValue(QStringLiteral("--"), 46);
    const QString crestTip = QStringLiteral(
        "Peak over RMS of the envelope, over the last %1 ms of signal. Pauses are left out.")
        .arg(SsbModulationAnalyzer::kCrestWindowMs);
    lblCrest->setToolTip(crestTip);
    m_crestValue->setToolTip(crestTip);
    m_pctValue   = insetValue(QStringLiteral("--"), 46);
    grid->addWidget(lblPeak,  0, 0); grid->addWidget(m_peakValue,  0, 1);
    grid->addWidget(lblAvg,   0, 2); grid->addWidget(m_avgValue,   0, 3);
    grid->addWidget(lblCrest, 1, 0); grid->addWidget(m_crestValue, 1, 1);
    grid->addWidget(lblPct,   1, 2); grid->addWidget(m_pctValue,   1, 3);
    vbox->addLayout(grid);

    auto* lampRow = new QHBoxLayout();
    lampRow->setSpacing(3);
    m_overFlasher = new QLabel(QStringLiteral("OVER"), body);
    m_overFlasher->setAlignment(Qt::AlignCenter);
    m_overFlasher->setMinimumHeight(18);
    m_overSpin = new QSpinBox(body);
    m_overSpin->setRange(-20, 0);
    m_overSpin->setSuffix(QStringLiteral(" dB"));
    m_overSpin->setToolTip(QStringLiteral(
        "Flasher threshold. The lamp lights when the held peak reaches this level."));
    m_overSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
    m_overSpin->setFixedWidth(64);
    lampRow->addWidget(m_overFlasher, 1);
    lampRow->addWidget(m_overSpin);
    vbox->addLayout(lampRow);

    m_lamp = new QLabel(QStringLiteral("USB / LSB"), body);
    m_lamp->setAlignment(Qt::AlignCenter);
    m_lamp->setMinimumHeight(18);
    vbox->addWidget(m_lamp);

    connect(m_overSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int db) {
        AppSettings::instance().setValue(QString::fromLatin1(kKeyOverDb), QString::number(db));
        if (m_scope) {
            m_scope->setOverThresholdPct(percentForDbfs(db));
        }
    });

    m_scope = new SsbModScopeWidget(body);
    vbox->addWidget(m_scope, 1);
    root->addWidget(body);

    setLamp(m_overFlasher, QStringLiteral("OVER"), Style::kInsetBg, Style::kTextInactive, Style::kInsetBorder);
    setLamp(m_lamp, QStringLiteral("USB / LSB"), Style::kInsetBg, Style::kTextInactive, Style::kInsetBorder);
}

void SsbModMonitorApplet::loadSettings()
{
    auto& s = AppSettings::instance();
    const int src = s.value(QString::fromLatin1(kKeySource), QStringLiteral("0")).toInt();
    const int over = s.value(QString::fromLatin1(kKeyOverDb), QStringLiteral("-1")).toInt();
    const bool vu = s.value(QString::fromLatin1(kKeyVintage), QStringLiteral("True")).toString()
                    != QLatin1String("False");
    {
        const QSignalBlocker b1(m_overSpin);
        m_overSpin->setValue(over);
    }
    m_scope->setOverThresholdPct(percentForDbfs(over));
    setSource(src == 1 ? Source::PaFeedback : Source::TxIq);
    {
        const QSignalBlocker block(m_vuBtn);
        m_vuBtn->setChecked(vu);
    }
    setVintageMeters(vu);
}

void SsbModMonitorApplet::setVintageMeters(bool on)
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

void SsbModMonitorApplet::setSource(Source src)
{
    m_source = src;
    m_srcTxBtn->setChecked(src == Source::TxIq);
    m_srcFbBtn->setChecked(src == Source::PaFeedback);
    AppSettings::instance().setValue(QString::fromLatin1(kKeySource),
                                     QString::number(static_cast<int>(src)));
    if (isRemoteWindow()) {
        // Nothing reaches the Core until the panel is shown. No reset either:
        // it would clear the source for every other reader of the Core.
        if (isVisible()) {
            watchOnCore(static_cast<int>(src));
        }
    } else {
        syncLocalMeasuring(isVisible());
        restartLocalWindow();
    }
}

void SsbModMonitorApplet::restartLocalWindow()
{
    if (m_model) {
        if (auto* analyzer = m_model->ssbModulationAnalyzer(static_cast<int>(m_source))) {
            analyzer->restartWindow(SsbModulationAnalyzer::Consumer::Panel);
        }
    }
}

void SsbModMonitorApplet::resetPeaks()
{
    if (isRemoteWindow()) {
        if (IStationLink* link = m_model->stationLink(); link && link->txSsbModMonitorAvailable()) {
            link->requestSsbModMonitorReset(static_cast<int>(m_source));
        }
    } else if (m_model) {
        if (auto* a = m_model->ssbModulationAnalyzer(static_cast<int>(m_source))) {
            a->reset();
        }
    }
    m_peakGauge->setPeakValue(-999.0);
    m_avgGauge->setPeakValue(-999.0);
    if (m_peakMeter) {
        m_peakMeter->setPeakValue(-1.0);
        m_peakMeter->setValue(0.0);
        m_peakMeter->setLit(false);
    }
    if (m_avgMeter) {
        m_avgMeter->setPeakValue(-1.0);
        m_avgMeter->setValue(0.0);
        m_avgMeter->setLit(false);
    }
    m_overLit = false;
    setLamp(m_overFlasher, QStringLiteral("OVER"), Style::kInsetBg, Style::kTextInactive, Style::kInsetBorder);
}

void SsbModMonitorApplet::syncFromModel()
{
}

void SsbModMonitorApplet::showEvent(QShowEvent* e)
{
    AppletWidget::showEvent(e);
    m_timer.start();
    watchOnCore(static_cast<int>(m_source));
    syncLocalMeasuring(true);
    if (!isRemoteWindow()) {
        restartLocalWindow();
    }
}

void SsbModMonitorApplet::hideEvent(QHideEvent* e)
{
    m_timer.stop();
    watchOnCore(-1);
    syncLocalMeasuring(false);
    AppletWidget::hideEvent(e);
}

void SsbModMonitorApplet::tick()
{
    if (!m_model) {
        return;
    }
    if (isRemoteWindow()) {
        // The Core already measures only while keyed in USB or LSB. Show
        // what it sends. A local mode check here was discarding those
        // readings, so the gauges and the scope stayed empty.
        if (m_available) {
            applySnapshot(m_model->stationSsbModMonitorSnapshot(static_cast<int>(m_source))
                              .value_or(SsbModulationAnalyzer::Snapshot{}));
        }
        return;
    }
    auto* analyzer = m_model->ssbModulationAnalyzer(static_cast<int>(m_source));
    if (!analyzer) {
        return;
    }
    applySnapshot(analyzer->snapshot(SsbModulationAnalyzer::Consumer::Panel));
}

void SsbModMonitorApplet::setLamp(QLabel* lamp, const QString& text,
                                  const char* bg, const char* fg, const char* border)
{
    lamp->setText(text);
    lamp->setStyleSheet(QStringLiteral(
        "QLabel { background:%1; color:%2; border:1px solid %3; border-radius:2px; "
        "font-weight:bold; font-size:10px; padding:1px 3px; }")
        .arg(QLatin1String(bg), QLatin1String(fg), QLatin1String(border)));
}

void SsbModMonitorApplet::applySnapshot(const SsbModulationAnalyzer::Snapshot& s)
{
    const bool live = s.signalPresent;
    const double peak = live ? std::max(s.peakPct, s.peakHoldPct) : 0.0;
    const double avg = live ? s.avgPct : 0.0;
    m_peakGauge->setValue(peak);
    m_avgGauge->setValue(avg);
    m_peakGauge->setPeakValue(live && s.peakHoldPct > 0.0 ? s.peakHoldPct : -999.0);
    m_avgGauge->setPeakValue(-999.0);
    if (m_peakMeter && m_avgMeter) {
        m_peakMeter->setLit(live);
        m_avgMeter->setLit(live);
        m_peakMeter->setValue(peak);
        m_avgMeter->setValue(avg);
        m_peakMeter->setPeakValue(live && s.peakHoldPct > 0.0 ? s.peakHoldPct : -1.0);
        m_avgMeter->setPeakValue(-1.0);
    }

    if (live) {
        m_peakValue->setText(QStringLiteral("%1").arg(s.peakHoldDbfs, 0, 'f', 1));
        m_avgValue->setText(QStringLiteral("%1").arg(s.avgDbfs, 0, 'f', 1));
        m_crestValue->setText(s.crestDb ? QStringLiteral("%1").arg(*s.crestDb, 0, 'f', 1)
                                        : QStringLiteral("--"));
        m_pctValue->setText(QStringLiteral("%1").arg(s.peakHoldPct, 0, 'f', 0));
    } else {
        m_peakValue->setText(QStringLiteral("--"));
        m_avgValue->setText(QStringLiteral("--"));
        m_crestValue->setText(QStringLiteral("--"));
        m_pctValue->setText(QStringLiteral("--"));
    }

    const double limit = overThresholdDbfs();
    const bool over = live && s.peakHoldDbfs >= limit;
    if (over != m_overLit) {
        m_overLit = over;
        if (over) {
            setLamp(m_overFlasher, QStringLiteral("OVER"), Style::kRedBg, Style::kRedText, Style::kRedBorder);
        } else {
            setLamp(m_overFlasher, QStringLiteral("OVER"), Style::kInsetBg, Style::kTextInactive, Style::kInsetBorder);
        }
    }
    if (!live) {
        setLamp(m_lamp, QStringLiteral("NO RF"), Style::kInsetBg, Style::kTextInactive, Style::kInsetBorder);
    } else if (over) {
        setLamp(m_lamp, QStringLiteral("OVER"), Style::kRedBg, Style::kRedText, Style::kRedBorder);
    } else {
        setLamp(m_lamp, QStringLiteral("ON"), Style::kGreenBg, Style::kGreenText, Style::kGreenBorder);
    }
    if (m_scope) {
        m_scope->setOverThresholdPct(percentForDbfs(limit));
        m_scope->setTrace(s.scope);
    }
}

} // namespace NereusSDR
