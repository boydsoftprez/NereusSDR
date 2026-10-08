// no-port-check: NereusSDR-original. The Rotor applet around the dial
// ported from Longpath (RotorDialWidget carries that port's header).

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/applets/RotorApplet.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original; no upstream port. Rotor control plan, Task 6. See
// RotorApplet.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08  J.J. Boyd / KG4VCF  Created (rotor control plan, Task 6).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-08  J.J. Boyd / KG4VCF  Rotor control plan Task 8: a remote
//                                    Core's refusal of the applet's own
//                                    command is the applet's to show while
//                                    it is on screen (the accessory rule);
//                                    otherwise a notice says it.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include "RotorApplet.h"

#include "core/RotorRoute.h"
#include "gui/widgets/RotorDialWidget.h"
#include "models/RadioModel.h"
#include "models/RotorModel.h"

#include <QButtonGroup>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <cmath>
#include <memory>

namespace NereusSDR {

namespace {

using RotorLink::RotorModel;

// The rotor mockup's colours.
constexpr const char* kButtonStyle =
    "QPushButton { background: #1a2a3a; border: 1px solid #205070; color: #c8d8e8;"
    " border-radius: 4px; padding: 4px 8px; font-size: 12px; }"
    "QPushButton:pressed, QPushButton:checked { background: #0070c0;"
    " border-color: #0090e0; color: #ffffff; }"
    "QPushButton:disabled { background: #141c26; border-color: #253040; color: #506070; }";
// Stop is the only red control.
constexpr const char* kStopStyle =
    "QPushButton { background: #7a1c1c; border: 1px solid #c14848; color: #ffd0d0;"
    " border-radius: 4px; padding: 4px 8px; font-size: 12px; font-weight: bold; }"
    "QPushButton:pressed { background: #9a2424; }"
    "QPushButton:disabled { background: #141c26; border-color: #253040; color: #506070;"
    " font-weight: normal; }";
constexpr const char* kPresetStyle =
    "QPushButton { background: #1a2a3a; border: 1px solid #205070; color: #c8d8e8;"
    " border-radius: 4px; padding: 3px 7px; font-size: 11px; }"
    "QPushButton:pressed { background: #0070c0; border-color: #0090e0; color: #ffffff; }"
    "QPushButton:disabled { background: #141c26; border-color: #253040; color: #506070; }";
constexpr const char* kCallStyle =
    "QLineEdit { background: #1a2a3a; border: 1px solid #304050; color: #c8d8e8;"
    " border-radius: 3px; padding: 4px 6px; font-size: 12px; }"
    "QLineEdit:disabled { background: #141c26; border-color: #253040; color: #506070; }";
constexpr const char* kSectionStyle =
    "QLabel { color: #607080; font-size: 10px; letter-spacing: 1px; }";
constexpr const char* kStatusStyle = "QLabel { color: #8090a0; font-size: 11px; }";
constexpr const char* kTargetStyle =
    "QLabel { color: #8090a0; font-size: 12px; font-family: Menlo, Consolas, monospace; }";
constexpr const char* kReasonStyle = "QLabel { color: #607080; font-size: 11px; }";

constexpr const char* kHeadingColour = "#ffb800";
constexpr const char* kHeadingUnknownColour = "#506070";
constexpr const char* kHeadingStaleColour = "#a6b0bc";

constexpr const char* kDotGreen = "#5fff8a";
constexpr const char* kDotAmber = "#ffb800";
constexpr const char* kDotGrey = "#404858";

// The readout calls a route this long the long way round, as the route
// planner does (RotorRoute::kLongWayDeg).
constexpr double kLongWayDeg = 270.0;

QString degrees(double deg)
{
    return QStringLiteral("%1°").arg(qRound(RotorRoute::wrap360(deg)) % 360, 3, 10,
                                          QLatin1Char('0'));
}

QString plainDegrees(double deg)
{
    return QStringLiteral("%1°").arg(qRound(deg));
}

struct Preset {
    QString name;
    double deg{0.0};
};

QList<Preset> parsePresets(const QString& text)
{
    QList<Preset> out;
    for (const QString& line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const qsizetype tab = line.lastIndexOf(QLatin1Char('\t'));
        bool ok = false;
        const double deg = (tab >= 0 ? line.mid(tab + 1) : line).trimmed().toDouble(&ok);
        if (!ok || deg < 0.0 || deg > 360.0) { continue; }
        out.append({tab >= 0 ? line.left(tab).trimmed() : QString(), deg});
    }
    return out;
}

} // namespace

RotorApplet::RotorApplet(RadioModel* model, QWidget* parent)
    : RotorApplet(model, model ? model->rotorModel() : nullptr, model, parent)
{
}

RotorApplet::RotorApplet(RadioModel* model, RotorLink::RotorModel* rotor,
                         RotorCommandSink* commands, QWidget* parent)
    : AppletWidget(model, parent)
    , m_rotor(rotor)
    , m_commands(commands)
{
    m_holdTimer.setInterval(kHoldRepeatMs);
    connect(&m_holdTimer, &QTimer::timeout, this, &RotorApplet::repeatHold);
    m_staleTicker.setInterval(1000);
    connect(&m_staleTicker, &QTimer::timeout, this, &RotorApplet::updateReadout);

    buildUi();
    bindRotor();

    if (model) {
        // A remote Core's link comes and goes (and with it whether the Core
        // controls a rotor at all); its verdicts on the commands follow.
        connect(model, &RadioModel::stationLinkStateChanged, this, &RotorApplet::syncFromModel);
        connect(model, &RadioModel::stationCommandFinished, this, &RotorApplet::onCommandFinished);
    }
    syncFromModel();
}

RotorApplet::~RotorApplet()
{
    // A hold never outlives the window that holds it.
    if (m_holding && m_commands) {
        m_holdTimer.stop();
        m_holding = false;
        m_commands->requestNudgeRotor(m_holdDirection, false, nullptr);
    }
}

QPushButton* RotorApplet::makeButton(const QString& text, const QString& objectName)
{
    auto* b = new QPushButton(text, this);
    b->setObjectName(objectName);
    b->setStyleSheet(QString::fromLatin1(kButtonStyle));
    b->setFocusPolicy(Qt::NoFocus);
    b->setMinimumHeight(24);
    return b;
}

void RotorApplet::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 6, 8, 8);
    root->setSpacing(4);
    // No appletTitleBar(): the host prepends its own from appletTitle().

    // Status line.
    auto* statusRow = new QHBoxLayout;
    statusRow->setSpacing(6);
    m_statusDot = new QLabel(this);
    m_statusDot->setObjectName(QStringLiteral("rotorStatusDot"));
    m_statusDot->setFixedSize(8, 8);
    m_statusText = new QLabel(this);
    m_statusText->setObjectName(QStringLiteral("rotorStatus"));
    m_statusText->setTextFormat(Qt::PlainText);
    m_statusText->setStyleSheet(QString::fromLatin1(kStatusStyle));
    statusRow->addWidget(m_statusDot, 0, Qt::AlignVCenter);
    statusRow->addWidget(m_statusText, 1);
    root->addLayout(statusRow);

    // The dial.
    m_dial = new RotorDialWidget(this);
    m_dial->setObjectName(QStringLiteral("rotorDial"));
    m_dial->setMinimumHeight(160);
    root->addWidget(m_dial, 1);
    connect(m_dial, &RotorDialWidget::targetReleased, this, [this](double az) {
        sendTarget(az, -1.0, true);
    });
    connect(m_dial, &RotorDialWidget::elevationReleased, this, [this](double el) {
        // An elevation alone: keep the azimuth where it is going, or where
        // it is (the contract has no "leave azimuth").
        double az = m_rotor ? m_rotor->targetAzimuthDeg() : -1.0;
        if (az < 0.0 && m_rotor) { az = m_rotor->azimuthDeg(); }
        if (az < 0.0) {
            m_dial->clearSelection();
            return;
        }
        sendTarget(az, el, true);
    });
    connect(m_dial, &RotorDialWidget::selectionChanged, this, &RotorApplet::updateReadout);

    // The readout: heading on the left, target and "to go" on the right.
    auto* readout = new QHBoxLayout;
    readout->setContentsMargins(2, 4, 2, 6);
    m_heading = new QLabel(this);
    m_heading->setObjectName(QStringLiteral("rotorHeading"));
    m_heading->setTextFormat(Qt::RichText);
    m_target = new QLabel(this);
    m_target->setObjectName(QStringLiteral("rotorTarget"));
    m_target->setTextFormat(Qt::PlainText);
    m_target->setStyleSheet(QString::fromLatin1(kTargetStyle));
    m_target->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    readout->addWidget(m_heading, 0, Qt::AlignBottom);
    readout->addStretch(1);
    readout->addWidget(m_target, 0, Qt::AlignBottom);
    root->addLayout(readout);

    // CCW / STOP / CW, held to turn; Stop at once.
    auto* turnRow = new QHBoxLayout;
    turnRow->setSpacing(6);
    m_ccw = makeButton(QStringLiteral("◀ CCW"), QStringLiteral("rotorCcw"));
    m_stop = makeButton(QStringLiteral("STOP"), QStringLiteral("rotorStop"));
    m_stop->setStyleSheet(QString::fromLatin1(kStopStyle));
    QFont stopFont = m_stop->font();
    stopFont.setLetterSpacing(QFont::AbsoluteSpacing, 0.8);
    m_stop->setFont(stopFont);
    m_cw = makeButton(QStringLiteral("CW ▶"), QStringLiteral("rotorCw"));
    turnRow->addWidget(m_ccw, 10);
    turnRow->addWidget(m_stop, 13);
    turnRow->addWidget(m_cw, 10);
    root->addLayout(turnRow);

    // Down / Up on an az/el rotor.
    m_elevationRow = new QWidget(this);
    auto* elRow = new QHBoxLayout(m_elevationRow);
    elRow->setContentsMargins(0, 2, 0, 0);
    elRow->setSpacing(6);
    m_down = makeButton(QStringLiteral("▼ Down"), QStringLiteral("rotorDown"));
    m_up = makeButton(QStringLiteral("▲ Up"), QStringLiteral("rotorUp"));
    elRow->addWidget(m_down, 1);
    elRow->addWidget(m_up, 1);
    root->addWidget(m_elevationRow);

    const struct { QPushButton* button; RotorCommandSink::Nudge direction; } kHolds[] = {
        {m_ccw, RotorCommandSink::Nudge::Ccw},
        {m_cw, RotorCommandSink::Nudge::Cw},
        {m_down, RotorCommandSink::Nudge::Down},
        {m_up, RotorCommandSink::Nudge::Up},
    };
    for (const auto& hold : kHolds) {
        const RotorCommandSink::Nudge direction = hold.direction;
        connect(hold.button, &QPushButton::pressed, this, [this, direction] { startHold(direction); });
        connect(hold.button, &QPushButton::released, this, &RotorApplet::endHold);
    }
    connect(m_stop, &QPushButton::clicked, this, &RotorApplet::sendStop);

    // Short path / long path (for "Turn to").
    auto* pathRow = new QHBoxLayout;
    pathRow->setContentsMargins(0, 2, 0, 0);
    pathRow->setSpacing(6);
    m_shortPath = makeButton(QStringLiteral("Short path"), QStringLiteral("rotorShortPath"));
    m_longPathButton = makeButton(QStringLiteral("Long path"), QStringLiteral("rotorLongPath"));
    m_shortPath->setCheckable(true);
    m_longPathButton->setCheckable(true);
    m_shortPath->setChecked(true);
    auto* pathGroup = new QButtonGroup(this);
    pathGroup->setExclusive(true);
    pathGroup->addButton(m_shortPath);
    pathGroup->addButton(m_longPathButton);
    connect(m_longPathButton, &QPushButton::toggled, this, [this](bool on) { m_longPath = on; });
    pathRow->addWidget(m_shortPath, 1);
    pathRow->addWidget(m_longPathButton, 1);
    root->addLayout(pathRow);

    // Presets.
    m_presetsLabel = new QLabel(QStringLiteral("PRESETS"), this);
    m_presetsLabel->setStyleSheet(QString::fromLatin1(kSectionStyle));
    root->addSpacing(4);
    root->addWidget(m_presetsLabel);
    m_presetsBox = new QWidget(this);
    m_presetsGrid = new QGridLayout(m_presetsBox);
    m_presetsGrid->setContentsMargins(0, 0, 0, 0);
    m_presetsGrid->setSpacing(4);
    root->addWidget(m_presetsBox);
    m_noPresets = new QLabel(QStringLiteral("No presets set up."), this);
    m_noPresets->setObjectName(QStringLiteral("rotorNoPresets"));
    m_noPresets->setStyleSheet(QString::fromLatin1(kReasonStyle));
    root->addWidget(m_noPresets);

    // Turn to a callsign.
    m_turnToLabel = new QLabel(QStringLiteral("TURN TO"), this);
    m_turnToLabel->setStyleSheet(QString::fromLatin1(kSectionStyle));
    root->addSpacing(4);
    root->addWidget(m_turnToLabel);
    auto* callRow = new QHBoxLayout;
    callRow->setSpacing(6);
    m_call = new QLineEdit(this);
    m_call->setObjectName(QStringLiteral("rotorCall"));
    m_call->setPlaceholderText(QStringLiteral("Callsign"));
    m_call->setStyleSheet(QString::fromLatin1(kCallStyle));
    m_turnTo = makeButton(QStringLiteral("Turn"), QStringLiteral("rotorTurnTo"));
    callRow->addWidget(m_call, 1);
    callRow->addWidget(m_turnTo);
    root->addLayout(callRow);
    connect(m_turnTo, &QPushButton::clicked, this, &RotorApplet::sendTurnToCall);
    connect(m_call, &QLineEdit::returnPressed, this, &RotorApplet::sendTurnToCall);

    // Why the controls are greyed, or what the Core said no to.
    m_reason = new QLabel(this);
    m_reason->setObjectName(QStringLiteral("rotorReason"));
    m_reason->setTextFormat(Qt::PlainText);
    m_reason->setWordWrap(true);
    m_reason->setStyleSheet(QString::fromLatin1(kReasonStyle));
    root->addWidget(m_reason);
}

void RotorApplet::bindRotor()
{
    m_dial->setRotorModel(m_rotor);
    if (!m_rotor) { return; }
    connect(m_rotor, &RotorModel::stateChanged, this, &RotorApplet::syncFromModel);
    connect(m_rotor, &RotorModel::positionChanged, this, [this] {
        updateStatus();
        updateReadout();
    });
}

void RotorApplet::syncFromModel()
{
    updateControls();
    updateStatus();
    updateReadout();
    rebuildPresets();
    updateMessage();
}

void RotorApplet::updateStatus()
{
    QString text = QStringLiteral("Not connected");
    const char* dot = kDotGrey;
    if (m_rotor && m_rotor->driver() != RotorModel::Driver::None) {
        const QString label = m_rotor->label().isEmpty() ? QStringLiteral("Rotor") : m_rotor->label();
        switch (m_rotor->connectionPhase()) {
        case TunerModel::ConnectionPhase::Connected:
            if (m_rotor->motion() != RotorModel::Motion::Stopped) {
                text = label + QStringLiteral(" · Turning");
                dot = kDotAmber;
            } else {
                text = label;
                dot = kDotGreen;
            }
            break;
        case TunerModel::ConnectionPhase::Discovering:
        case TunerModel::ConnectionPhase::Connecting:
        case TunerModel::ConnectionPhase::Identifying:
        case TunerModel::ConnectionPhase::Retrying:
            text = label + QStringLiteral(" · Connecting");
            dot = kDotAmber;
            break;
        default:
            break;
        }
    }
    m_statusText->setText(text);
    const QString colour = QString::fromLatin1(dot);
    const QString glow = dot == kDotGrey ? QString()
                                         : QStringLiteral(" border: 1px solid %1;").arg(colour);
    m_statusDot->setStyleSheet(
        QStringLiteral("QLabel { background: %1; border-radius: 4px;%2 }").arg(colour, glow));
}

void RotorApplet::updateReadout()
{
    const bool connected = m_rotor && m_rotor->driver() != RotorModel::Driver::None
        && m_rotor->connectionPhase() == TunerModel::ConnectionPhase::Connected;
    const double az = m_rotor ? m_rotor->azimuthDeg() : -1.0;
    const double el = m_rotor ? m_rotor->elevationDeg() : -1.0;
    const bool azEl = m_rotor && m_rotor->axes() == RotorModel::Axes::AzimuthElevation;
    const bool known = connected && az >= 0.0;
    const bool fresh = known && m_rotor->positionFresh();

    // How long since the rotor last answered.
    if (fresh) {
        m_lastFresh.restart();
        m_staleOffsetMs = 0;
        m_seenStale = false;
    } else if (known && !m_seenStale) {
        m_seenStale = true;
        if (!m_lastFresh.isValid()) {
            // Stale before this window heard it at all: it went quiet at
            // least the stale threshold ago.
            m_lastFresh.start();
            m_staleOffsetMs = kStaleAfterMs;
        }
    }
    if (known && !fresh) {
        if (!m_staleTicker.isActive()) { m_staleTicker.start(); }
    } else {
        m_staleTicker.stop();
    }

    // The heading.
    const char* colour = !known ? kHeadingUnknownColour
                       : fresh  ? kHeadingColour
                                : kHeadingStaleColour;
    QString heading = known ? degrees(az) : QStringLiteral("---°");
    if (known && azEl && el >= 0.0) {
        heading += QStringLiteral(" <span style=\"font-size:16px\">el %1</span>")
                       .arg(plainDegrees(el));
    }
    m_heading->setText(QStringLiteral(
        "<span style=\"font-family:Menlo,Consolas,monospace; font-size:26px;"
        " font-weight:600; color:%1\">%2</span>").arg(QString::fromLatin1(colour), heading));

    // The target and "to go".
    QString target;
    if (known && !fresh) {
        const qint64 ago = m_lastFresh.elapsed() + m_staleOffsetMs;
        target = QStringLiteral("Last heard %1s ago").arg(std::max<qint64>(1, ago / 1000));
    } else if (known) {
        const bool tape = m_dial->shape() == RotorDialWidget::Shape::Tape;
        const double selection = m_dial->selectionDeg();
        const double targetAz = m_dial->selecting() && !m_dial->selectingElevation()
            ? selection
            : m_rotor->targetAzimuthDeg();
        const double targetEl = m_dial->selectingElevation() ? selection
                                                             : m_rotor->targetElevationDeg();
        const bool selecting = m_dial->selecting();
        auto direction = [tape](double travel) {
            if (!tape) { return QString(); }
            return travel >= 0.0 ? QStringLiteral("CW ") : QStringLiteral("CCW ");
        };
        if (targetAz >= 0.0
            && std::abs(RotorRoute::planFree(az, targetAz).travelDeg)
                   > RotorDialWidget::kArrivalToleranceDeg) {
            double travel = 0.0;
            bool routeKnown = true;
            if (selecting) {
                const auto stop = static_cast<RotorRoute::EndStop>(m_rotor->endStop());
                const RotorRoute::Move move = stop == RotorRoute::EndStop::None
                    ? RotorRoute::planFree(az, targetAz)
                    : RotorRoute::planOnSpan(m_rotor->spanPositionDeg(), targetAz, stop,
                                             m_rotor->rangeDeg());
                travel = move.travelDeg;
                routeKnown = move.routeKnown;
            } else {
                travel = m_rotor->travelDeg();
                routeKnown = m_rotor->routeKnown();
            }
            if (!routeKnown) {
                target = QStringLiteral("%1 · route known once the rotor moves")
                             .arg(degrees(targetAz));
            } else {
                target = QStringLiteral("%1 · %2%3 to go")
                             .arg(degrees(targetAz), direction(travel),
                                  plainDegrees(std::abs(travel)));
                if (std::abs(travel) > kLongWayDeg) {
                    target += QStringLiteral(" · long way round");
                }
            }
        } else if (azEl && targetEl >= 0.0 && el >= 0.0
                   && std::abs(targetEl - el) > RotorDialWidget::kArrivalToleranceDeg) {
            target = QStringLiteral("el %1 · %2 to go")
                         .arg(plainDegrees(targetEl), plainDegrees(std::abs(targetEl - el)));
        } else if (m_dial->state() == RotorDialWidget::State::OnTarget) {
            target = QStringLiteral("on target");
        }
    }
    m_target->setText(target);
}

void RotorApplet::updateControls()
{
    QString reason;
    const bool available = m_commands && m_commands->rotorControlAvailable(&reason);
    if (!m_commands) {
        reason = QStringLiteral("No rotor is set up on this Core.");
    } else if (available) {
        reason.clear();
        if (!m_rotor || m_rotor->driver() == RotorModel::Driver::None) {
            reason = QStringLiteral("No rotor is set up on this Core.");
        } else if (m_rotor->connectionPhase() != TunerModel::ConnectionPhase::Connected) {
            reason = QStringLiteral("The rotor is not connected.");
        }
    }
    m_disabledReason = reason;
    const bool enabled = reason.isEmpty();
    if (!enabled && m_holding) { endHold(); }
    m_controlsEnabled = enabled;

    for (QWidget* w : {static_cast<QWidget*>(m_dial), static_cast<QWidget*>(m_ccw),
                       static_cast<QWidget*>(m_stop), static_cast<QWidget*>(m_cw),
                       static_cast<QWidget*>(m_down), static_cast<QWidget*>(m_up),
                       static_cast<QWidget*>(m_shortPath),
                       static_cast<QWidget*>(m_longPathButton),
                       static_cast<QWidget*>(m_presetsBox), static_cast<QWidget*>(m_call),
                       static_cast<QWidget*>(m_turnTo)}) {
        w->setEnabled(enabled);
    }

    // Down / Up belong to an az/el rotor; an azimuth rotor has no such
    // buttons (the mockup's azimuth applet).
    const bool azEl = m_rotor && m_rotor->axes() == RotorModel::Axes::AzimuthElevation;
    m_elevationRow->setVisible(azEl);
}

void RotorApplet::rebuildPresets()
{
    const QString text = m_rotor ? m_rotor->presets() : QString();
    const QList<Preset> presets = parsePresets(text);
    m_noPresets->setVisible(presets.isEmpty());
    m_presetsBox->setVisible(!presets.isEmpty());
    if (text == m_presetsShown && m_presetsGrid->count() == presets.size()) { return; }
    m_presetsShown = text;
    while (QLayoutItem* taken = m_presetsGrid->takeAt(0)) {
        const std::unique_ptr<QLayoutItem> item(taken);
        if (QWidget* w = item->widget()) { w->deleteLater(); }
    }
    constexpr int kColumns = 3;
    for (int i = 0; i < presets.size(); ++i) {
        const Preset& p = presets.at(i);
        const QString label = p.name.isEmpty() ? plainDegrees(p.deg)
                                               : p.name + QLatin1Char(' ') + plainDegrees(p.deg);
        auto* b = new QPushButton(label, m_presetsBox);
        b->setObjectName(QStringLiteral("rotorPreset"));
        b->setStyleSheet(QString::fromLatin1(kPresetStyle));
        b->setFocusPolicy(Qt::NoFocus);
        const double deg = p.deg;
        connect(b, &QPushButton::clicked, this, [this, deg] { sendTarget(deg, -1.0, false); });
        m_presetsGrid->addWidget(b, i / kColumns, i % kColumns);
    }
}

void RotorApplet::updateMessage()
{
    m_reason->setText(m_disabledReason.isEmpty() ? m_message : m_disabledReason);
    m_reason->setVisible(!m_reason->text().isEmpty());
}

// ── The hold dead man ───────────────────────────────────────────────
//
// The contract's nudgeRotor: active true on press and every 250 ms while
// held, active false on release. The Core stops a hold that goes quiet for
// 750 ms, so a window that dies mid-hold cannot leave the rotor turning.

void RotorApplet::startHold(RotorCommandSink::Nudge direction)
{
    if (!m_commands || !m_controlsEnabled) { return; }
    if (m_holding) { endHold(); }
    m_holding = true;
    m_holdDirection = direction;
    QString reason;
    const bool sent = m_commands->requestNudgeRotor(direction, true, &reason);
    noteSent(sent, reason, false);
    if (!sent) {
        m_holding = false;
        return;
    }
    m_holdTimer.start();
}

void RotorApplet::repeatHold()
{
    if (!m_holding || !m_commands) {
        m_holdTimer.stop();
        return;
    }
    QString reason;
    if (!m_commands->requestNudgeRotor(m_holdDirection, true, &reason)) {
        m_holdTimer.stop();
        m_holding = false;
        noteSent(false, reason, false);
    }
}

void RotorApplet::endHold()
{
    m_holdTimer.stop();
    if (!m_holding) { return; }
    m_holding = false;
    if (!m_commands) { return; }
    QString reason;
    const bool sent = m_commands->requestNudgeRotor(m_holdDirection, false, &reason);
    if (!sent) { noteSent(false, reason, false); }
}

// ── One-shot commands ───────────────────────────────────────────────

void RotorApplet::sendTarget(double azimuthDeg, double elevationDeg, bool fromDial)
{
    if (!m_commands || !m_controlsEnabled) {
        if (fromDial) { m_dial->clearSelection(); }
        return;
    }
    QString reason;
    const bool sent = m_commands->requestRotorTarget(azimuthDeg, elevationDeg, &reason);
    noteSent(sent, reason, fromDial);
}

void RotorApplet::sendStop()
{
    if (!m_commands) { return; }
    if (m_holding) { endHold(); }
    QString reason;
    const bool sent = m_commands->requestStopRotor(&reason);
    noteSent(sent, reason, false);
    m_dial->clearSelection();
}

void RotorApplet::sendTurnToCall()
{
    const QString call = m_call->text().trimmed().toUpper();
    if (call.isEmpty() || !m_commands || !m_controlsEnabled) { return; }
    QString reason;
    const bool sent = m_commands->requestTurnRotorToCall(call, m_longPath, &reason);
    noteSent(sent, reason, false);
}

void RotorApplet::noteSent(bool sent, const QString& reason, bool fromDial)
{
    m_pendingCommandId = sent && m_commands ? m_commands->lastRotorCommandId() : 0;
    m_pendingFromDial = fromDial;
    // Rotor control plan Task 8: the Core's refusal comes the accessory way
    // (device "rotor"); the applet shows it itself while on screen.
    if (m_pendingCommandId != 0 && m_model) {
        m_model->noteAccessoryRequestShownOnPage(m_pendingCommandId, this);
    }
    m_message = sent ? QString() : reason;
    if (!sent && fromDial) { m_dial->clearSelection(); }
    updateMessage();
}

void RotorApplet::onCommandFinished(quint32 commandId, bool accepted, const QString& reason)
{
    if (commandId == 0 || commandId != m_pendingCommandId) { return; }
    m_pendingCommandId = 0;
    if (accepted) { return; }
    m_message = reason;
    if (m_pendingFromDial) { m_dial->clearSelection(); }
    updateMessage();
}

} // namespace NereusSDR
