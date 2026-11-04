// =================================================================
// src/gui/applets/SsbModMonitorApplet.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original file. See the header.
//
// Modification history (NereusSDR):
//   2026-11-04 — Created. J.J. Boyd (KG4VCF), AI-assisted via xAI Grok.
// =================================================================
#include "gui/applets/SsbModMonitorApplet.h"

#include "core/AppSettings.h"
#include "core/WdspTypes.h"
#include "core/session/IStationLink.h"
#include "gui/HGauge.h"
#include "gui/StyleConstants.h"
#include "gui/styles/WorkspacePreview.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {
constexpr auto kKeySource   = "SsbModMon/Source";
constexpr auto kKeyOverDb   = "SsbModMon/OverDb";
constexpr auto kKeyFbStream = "SsbModMon/FbStream";
constexpr int  kRefreshMs   = 33;
constexpr double kScopeMax  = 120.0;

double percentForDbfs(double dbfs)
{
    return 100.0 * std::pow(10.0, dbfs / 20.0);
}
}

SsbModScopeWidget::SsbModScopeWidget(QWidget* parent) : QWidget(parent)
{
    setMinimumHeight(60);
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

void SsbModScopeWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
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
    buildUI();
    loadSettings();
    m_timer.setInterval(kRefreshMs);
    connect(&m_timer, &QTimer::timeout, this, &SsbModMonitorApplet::tick);
    if (isRemoteWindow()) {
        connect(m_model, &RadioModel::stationLinkStateChanged,
                this, &SsbModMonitorApplet::updateRemoteAvailability);
        updateRemoteAvailability();
    }
}

bool SsbModMonitorApplet::isRemoteWindow() const
{
    return m_model && m_model->role() == RadioModel::Role::Remote;
}

bool SsbModMonitorApplet::txIsSsbVoice() const
{
    if (!m_model) {
        return false;
    }
    const SliceModel* slice = m_model->txBoundSlice();
    if (slice == nullptr) {
        return false;
    }
    const DSPMode mode = slice->dspMode();
    return mode == DSPMode::USB || mode == DSPMode::LSB;
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

double SsbModMonitorApplet::overThresholdDbfs() const
{
    return m_overSpin ? m_overSpin->value() : -1.0;
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
    m_srcFbBtn->setToolTip(QStringLiteral(
        "Measure the PA output via the PureSignal feedback receiver."));
    m_fbStreamSpin = new QSpinBox(body);
    m_fbStreamSpin->setRange(0, 4);
    m_fbStreamSpin->setPrefix(QStringLiteral("rx"));
    m_fbStreamSpin->setToolTip(QStringLiteral(
        "Receiver stream carrying the PureSignal feedback. Hermes Lite 2: rx1."));
    m_fbStreamSpin->setStyleSheet(QString::fromLatin1(Style::kSpinBoxStyle));
    m_fbStreamSpin->setFixedWidth(46);
    m_resetBtn = styledButton(QStringLiteral("RESET"), 52);
    m_resetBtn->setToolTip(QStringLiteral("Clear peak-hold and the flasher."));
    srcRow->addWidget(m_srcTxBtn);
    srcRow->addWidget(m_srcFbBtn);
    srcRow->addWidget(m_fbStreamSpin);
    srcRow->addStretch(1);
    srcRow->addWidget(m_resetBtn);
    vbox->addLayout(srcRow);

    connect(m_srcTxBtn, &QPushButton::clicked, this, [this]() { setSource(Source::TxIq); });
    connect(m_srcFbBtn, &QPushButton::clicked, this, [this]() { setSource(Source::PaFeedback); });
    connect(m_resetBtn, &QPushButton::clicked, this, &SsbModMonitorApplet::resetPeaks);
    connect(m_fbStreamSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
        AppSettings::instance().setValue(QString::fromLatin1(kKeyFbStream), QString::number(v));
        if (m_model) {
            m_model->setSsbModFeedbackStream(v);
        }
    });

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
    vbox->addWidget(m_peakGauge);
    vbox->addWidget(m_avgGauge);

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
    const int fb = s.value(QString::fromLatin1(kKeyFbStream), QStringLiteral("1")).toInt();
    {
        const QSignalBlocker b1(m_overSpin);
        const QSignalBlocker b2(m_fbStreamSpin);
        m_overSpin->setValue(over);
        m_fbStreamSpin->setValue(fb);
    }
    m_scope->setOverThresholdPct(percentForDbfs(over));
    if (m_model) {
        m_model->setSsbModFeedbackStream(fb);
    }
    setSource(src == 1 ? Source::PaFeedback : Source::TxIq);
}

void SsbModMonitorApplet::setSource(Source src)
{
    m_source = src;
    m_srcTxBtn->setChecked(src == Source::TxIq);
    m_srcFbBtn->setChecked(src == Source::PaFeedback);
    AppSettings::instance().setValue(QString::fromLatin1(kKeySource),
                                     QString::number(static_cast<int>(src)));
    if (isRemoteWindow()) {
        if (isVisible()) {
            watchOnCore(static_cast<int>(src));
        }
    } else if (m_model) {
        m_model->setSsbModFeedbackWanted(src == Source::PaFeedback);
    }
    resetPeaks();
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
}

void SsbModMonitorApplet::hideEvent(QHideEvent* e)
{
    m_timer.stop();
    watchOnCore(-1);
    AppletWidget::hideEvent(e);
}

void SsbModMonitorApplet::tick()
{
    if (!m_model) {
        return;
    }
    if (isRemoteWindow()) {
        if (!m_available) {
            return;
        }
        if (!txIsSsbVoice()) {
            applySnapshot(SsbModulationAnalyzer::Snapshot{});
            setLamp(m_lamp, QStringLiteral("USB / LSB"), Style::kInsetBg, Style::kTextInactive,
                    Style::kInsetBorder);
            return;
        }
        applySnapshot(m_model->stationSsbModMonitorSnapshot(static_cast<int>(m_source))
                          .value_or(SsbModulationAnalyzer::Snapshot{}));
        return;
    }
    if (!txIsSsbVoice()) {
        m_model->refreshSsbModulationTap();
        applySnapshot(SsbModulationAnalyzer::Snapshot{});
        setLamp(m_lamp, QStringLiteral("USB / LSB"), Style::kInsetBg, Style::kTextInactive,
                Style::kInsetBorder);
        return;
    }
    m_model->refreshSsbModulationTap();
    auto* analyzer = m_model->ssbModulationAnalyzer(static_cast<int>(m_source));
    if (!analyzer) {
        return;
    }
    applySnapshot(analyzer->snapshot());
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
    const double peak = live ? s.peakPct : 0.0;
    const double avg = live ? s.avgPct : 0.0;
    m_peakGauge->setValue(peak);
    m_avgGauge->setValue(avg);
    m_peakGauge->setPeakValue(live && s.peakHoldPct > 0.0 ? s.peakHoldPct : -999.0);
    m_avgGauge->setPeakValue(-999.0);

    if (live) {
        m_peakValue->setText(QStringLiteral("%1").arg(s.peakHoldDbfs, 0, 'f', 1));
        m_avgValue->setText(QStringLiteral("%1").arg(s.avgDbfs, 0, 'f', 1));
        m_crestValue->setText(QStringLiteral("%1").arg(s.crestDb, 0, 'f', 1));
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
        m_scope->setTrace(live ? s.scope : std::vector<float>{});
    }
}

} // namespace NereusSDR
