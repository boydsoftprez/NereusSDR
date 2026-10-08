// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/widgets/RotorDialWidget.cpp  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   src/gui/widgets/RotorDialWidget.cpp [@551576e], original header from
//   Longpath source is included below.
//
// Longpath (Martin Fischer, OE5SOS, https://github.com/oe5sos/Longpath)
// is a fork of NereusSDR distributed under the GNU General Public
// License version 3 (its root LICENSE). Upstream source has no
// top-of-file GPL header; project-level LICENSE applies.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08: Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted transformation via Anthropic
//               Claude Code. Rotor control plan, Task 6. See
//               RotorDialWidget.h for what changed. The face geometry
//               (rings, three-step ticks, labels, travel pie, beam wedge,
//               tapered needle with its counterweight and halo, dashed
//               target with its halo, rim triangle, gradient hub) and the
//               tape follow paintFace() and paintTape(); the face is laid
//               out as the rotor mockup lays it out, with the readout in
//               the applet.
// =================================================================
//
// --- From RotorDialWidget.cpp ---
//
// =================================================================
// src/gui/widgets/RotorDialWidget.cpp  (Longpath)
// =================================================================
//
// Longpath-original — see RotorDialWidget.h for provenance.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "gui/widgets/RotorDialWidget.h"

#include "core/AppSettings.h"
#include "core/RotorRoute.h"
#include "models/RotorModel.h"

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

using RotorLink::RotorModel;
using RotorRoute::wrap360;

// From Longpath src/gui/widgets/RotorDialWidget.cpp:36-81 [@551576e]
// Palette taken from Style:: so the dial matches the rest of the app
// rather than carrying its own look. The four needle states reuse the
// existing semantic colours: accent for the target, amber for motion,
// green for arrival.
// ── Dieselbe Handschrift wie die uebrigen Instrumente ────────────────
//
// Der Betreiber, 2026-08-20: „weiters sieht die grafik des rotors
// nicht im stil der anderen grafiken aus, bitte aendern."
//
// Er hat recht, und der Unterschied ist benennbar: die Zeiger- und
// Balkeninstrumente fuehren jeden GEMESSENEN Wert in Bernstein
// (Instrument::measured(), Rolle „measured"), die Teilung in
// kTextScale und die Beschriftung in der Schmalschrift
// Style::monoFont. Der Rotor zeichnete seinen Zeiger in kTextPrimary —
// dasselbe Grau wie ein Beschriftungstext. Neben einem
// Stehwellenzeiger in Bernstein sieht das aus wie ein Bauteil aus
// einem anderen Programm.
//
// Die Richtung, in die die Antenne zeigt, IST eine Messung. Sie
// bekommt dieselbe Farbe wie jede andere.
//
// Das Ziel bleibt im Akzentblau: es ist keine Messung, sondern eine
// Vorgabe, und der Unterschied zwischen „wo sie steht" und „wo sie
// hin soll" ist genau der, den man auf einen Blick lesen will.
// NereusSDR: the colours are the rotor mockup's
// (docs/architecture/2026-10-07-rotor-control-mockup.html).
const QColor kActual    (QStringLiteral("#ffb800"));   // where it is — gemessen
const QColor kTarget    (QStringLiteral("#00b4d8"));   // where it should go
const QColor kTurning   (QStringLiteral("#ffb800"));   // in motion
const QColor kArrived   (QStringLiteral("#5fff8a"));   // on target
// ── Ring und Teilung eine Stufe heller ───────────────────────────────
//
// Der Betreiber, 2026-08-20: „grafik rotor auch leicht aufhellen."
//
// kBorder und kBorderSubtle sind RANDfarben — sie trennen Flaechen und
// duerfen dabei leise sein. Auf einem Zifferblatt sind dieselben
// Striche aber die TEILUNG, also das, was man ablesen soll. Eine
// Randfarbe als Skala ist zu zurueckhaltend fuer ihre Aufgabe.
//
// Eine Stufe hoeher in derselben Leiter: kTextScale fuer den aeusseren
// Ring, kBorder fuer den inneren. Die Abstufung zwischen beiden
// bleibt, sie liegt nur hoeher.
const QColor kRing      (QStringLiteral("#7e8a96"));
const QColor kRingInner (QStringLiteral("#2c3a48"));
const QColor kCardinal  (QStringLiteral("#a6b0bc"));
const QColor kMuted     (QStringLiteral("#a6b0bc"));
// NereusSDR: the four compass letters on the tape, brighter than the
// numbers between them (the mockup's #dcdce1).
const QColor kTapeCardinalText(QStringLiteral("#dcdce1"));

// Disabled (no rotor, a Core too old): the face stays, faded, as the
// mockup draws it.
constexpr double kDisabledOpacity = 0.45;

// The face's proportions, from the mockup's 200-unit rose: radius 88; on
// an az/el rotor the face is 300 units wide and the elevation gauge's
// corner sits at (208, 178) with radius 84.
constexpr double kRoseRadiusOfUnit     = 0.44;
constexpr double kElevationFaceAspect  = 1.5;
constexpr double kGaugeCornerXOfUnit   = 1.04;
constexpr double kGaugeCornerYOfUnit   = 0.89;
constexpr double kGaugeRadiusOfUnit    = 0.42;
constexpr double kMaxElevationDeg      = 90.0;

QFont monoFont(int pixelSize)
{
    // Longpath's Style::monoFont: figures that stand in a column want a
    // font whose digits are all one width.
    QFont f;
    f.setFamilies({QStringLiteral("Menlo"), QStringLiteral("Consolas"),
                   QStringLiteral("DejaVu Sans Mono")});
    f.setStyleHint(QFont::Monospace);
    f.setPixelSize(pixelSize);
    return f;
}

} // namespace

RotorDialWidget::RotorDialWidget(QWidget* parent)
    : QWidget(parent)
{
    setCursor(Qt::CrossCursor);
    setMouseTracking(false);

    // From Longpath src/gui/widgets/RotorDialWidget.cpp:119-126 [@551576e]
    // Die gemerkte Form. Vorgabe Vollkreis — so hat es der Betreiber
    // am 2026-08-21 entschieden („beide zur auswahl, standard
    // vollkreis").
    m_shape = AppSettings::instance()
                  .value(QString::fromLatin1(kShapeSettingsKey),
                         QStringLiteral("Rose")).toString()
                      == QStringLiteral("Tape")
                  ? Shape::Tape : Shape::Rose;

    // From Longpath src/gui/widgets/RotorDialWidget.cpp:128-130 [@551576e]
    // Die Wahl gehoert an das Ding selbst, nicht in einen Dialog: man
    // entscheidet sie, waehrend man es ansieht.
    setContextMenuPolicy(Qt::DefaultContextMenu);

    m_pendingTimer.setSingleShot(true);
    connect(&m_pendingTimer, &QTimer::timeout, this, [this] {
        m_pendingAzimuth = -1.0;
        m_pendingElevation = -1.0;
        update();
    });
}

RotorDialWidget::~RotorDialWidget() = default;

void RotorDialWidget::setRotorModel(RotorLink::RotorModel* model)
{
    if (m_model == model) { return; }
    disconnect(m_stateConnection);
    disconnect(m_positionConnection);
    m_model = model;
    if (model) {
        m_stateConnection = connect(model, &RotorModel::stateChanged,
                                    this, &RotorDialWidget::refreshFromModel);
        m_positionConnection = connect(model, &RotorModel::positionChanged,
                                       this, &RotorDialWidget::refreshFromModel);
    }
    refreshFromModel();
}

void RotorDialWidget::refreshFromModel()
{
    const bool have = !m_model.isNull();
    m_actual = have ? m_model->azimuthDeg() : -1.0;
    m_elevationAxis = have && m_model->axes() == RotorModel::Axes::AzimuthElevation;
    m_elevation = have ? m_model->elevationDeg() : -1.0;
    const double target = have ? m_model->targetAzimuthDeg() : -1.0;
    m_targetElevation = have ? m_model->targetElevationDeg() : -1.0;
    m_moving = have && m_model->motion() != RotorModel::Motion::Stopped;
    m_travel = have ? m_model->travelDeg() : 0.0;
    m_routeKnown = !have || m_model->routeKnown();
    m_fresh = have && m_model->positionFresh();
    // No end stop to mark while no rotor is set up.
    m_endStop = have && m_model->driver() != RotorModel::Driver::None
        ? static_cast<int>(m_model->endStop()) : 0;
    m_rangeDeg = have ? m_model->rangeDeg() : 360.0;
    m_spanDeg = have ? m_model->spanPositionDeg() : -1.0;

    // A target the Core reports replaces one sent on release.
    if (target >= 0.0 && target != m_target) {
        m_pendingAzimuth = -1.0;
    }
    if (m_targetElevation >= 0.0) {
        m_pendingElevation = -1.0;
    }
    if (m_pendingAzimuth < 0.0 && m_pendingElevation < 0.0) {
        m_pendingTimer.stop();
    }

    // The Core drops its target on arrival; keep it so the arrival shows
    // in green until the rotor moves off it or a new target comes.
    if (target >= 0.0) {
        m_lastTarget = target;
        m_arrivedAt = -1.0;
    } else if (m_target >= 0.0 && m_actual >= 0.0
               && std::abs(RotorRoute::planFree(m_actual, m_target).travelDeg)
                      <= kArrivalToleranceDeg) {
        m_arrivedAt = m_target;
    }
    if (m_arrivedAt >= 0.0
        && (m_actual < 0.0
            || std::abs(RotorRoute::planFree(m_actual, m_arrivedAt).travelDeg)
                   > kArrivalToleranceDeg)) {
        m_arrivedAt = -1.0;
    }
    m_target = target;
    update();
}

// From Longpath src/gui/widgets/RotorDialWidget.cpp:133-165 [@551576e]
void RotorDialWidget::contextMenuEvent(QContextMenuEvent* ev)
{
    QMenu menu(this);
    const QPointer<RotorDialWidget> self(this);
    auto* group = new QActionGroup(&menu);
    group->setExclusive(true);

    const struct { const char* label; Shape shape; const char* tip; } kForms[] = {
        {QT_TR_NOOP("Compass rose"), Shape::Rose,
         QT_TR_NOOP("The whole compass, north at the top. It also shows what "
                    "lies behind the antenna.")},
        {QT_TR_NOOP("Tape"), Shape::Tape,
         QT_TR_NOOP("A strip around the antenna instead of a circle. It uses "
                    "the full width but does not show all the way round.")},
    };
    for (const auto& f : kForms) {
        QAction* a = menu.addAction(tr(f.label));
        a->setCheckable(true);
        a->setChecked(m_shape == f.shape);
        a->setToolTip(tr(f.tip));
        group->addAction(a);
        const Shape target = f.shape;
        connect(a, &QAction::triggered, this, [this, target]() {
            setShape(target);
        });
    }
    menu.exec(ev->globalPos());
    // Das Elternteil kann waehrend exec() gestorben sein — dann ist
    // auch das Menue weg und `this` eine Leiche. Siehe ScopedChildWidget.h.
    if (!self) { return; }
    ev->accept();
}

// From Longpath src/gui/widgets/RotorDialWidget.cpp:167 [@551576e]
// NereusSDR: the applet's dial as the mockup sizes it (230 high).
QSize RotorDialWidget::sizeHint() const
{
    return showsElevation() ? QSize(300, 200) : QSize(230, 230);
}

// From Longpath src/gui/widgets/RotorDialWidget.cpp:169-174 [@551576e]
// Small enough that the panel can be dragged down to a compass and
// nothing else. Below about this the ticks stop being distinguishable
// and the needles overlap the hub, so it is a floor rather than a
// preference. (2026-08-10 — was {130, 150}, which set the panel's
// minimum height high enough that the dial was the thing squeezed.)
QSize RotorDialWidget::minimumSizeHint() const { return {84, 84}; }

// From Longpath src/gui/widgets/RotorDialWidget.cpp:176-181 [@551576e]
double RotorDialWidget::bearingToRadians(double deg)
{
    // Compass 0° is up and increases clockwise; Qt's maths angle is 0°
    // to the right and increases counter-clockwise.
    return qDegreesToRadians(90.0 - deg);
}

// ── Geometry, shared by drawing and hit-testing ─────────────────────
//
// NereusSDR: the mockup's face. A square for the rose, half as wide again
// on an az/el rotor for the elevation gauge, as large as the widget
// allows and centred in it.

bool RotorDialWidget::showsElevation() const
{
    return m_elevationAxis && m_shape == Shape::Rose;
}

double RotorDialWidget::faceUnit() const
{
    const double aspect = showsElevation() ? kElevationFaceAspect : 1.0;
    return std::max(1.0, std::min(static_cast<double>(height()), width() / aspect));
}

double RotorDialWidget::faceLeft() const
{
    const double aspect = showsElevation() ? kElevationFaceAspect : 1.0;
    return (width() - faceUnit() * aspect) / 2.0;
}

double RotorDialWidget::faceTop() const
{
    return (height() - faceUnit()) / 2.0;
}

QPointF RotorDialWidget::roseCentre() const
{
    return {faceLeft() + faceUnit() * 0.5, faceTop() + faceUnit() * 0.5};
}

double RotorDialWidget::roseRadius() const
{
    return faceUnit() * kRoseRadiusOfUnit;
}

QPointF RotorDialWidget::gaugeCorner() const
{
    return {faceLeft() + faceUnit() * kGaugeCornerXOfUnit,
            faceTop() + faceUnit() * kGaugeCornerYOfUnit};
}

double RotorDialWidget::gaugeRadius() const
{
    return faceUnit() * kGaugeRadiusOfUnit;
}

RotorDialWidget::State RotorDialWidget::state() const
{
    // From Longpath src/gui/widgets/RotorDialWidget.cpp:331-337 [@551576e]
    // (recomputeState), on the Core's target and motion.
    const double aimed = aimedAzimuth();
    if (aimed < 0.0) {
        return m_arrivedAt >= 0.0 ? State::OnTarget : State::Idle;
    }
    if (m_selecting != Selecting::None || m_pendingAzimuth >= 0.0) {
        return State::Targeted;
    }
    if (m_actual >= 0.0
        && std::abs(RotorRoute::planFree(m_actual, aimed).travelDeg) <= kArrivalToleranceDeg) {
        return State::OnTarget;
    }
    return m_moving ? State::Turning : State::Targeted;
}

double RotorDialWidget::selectionDeg() const
{
    return m_selecting != Selecting::None ? m_selection : -1.0;
}

void RotorDialWidget::clearSelection()
{
    m_pendingAzimuth = -1.0;
    m_pendingElevation = -1.0;
    m_pendingTimer.stop();
    update();
}

double RotorDialWidget::aimedAzimuth() const
{
    if (m_selecting == Selecting::Azimuth) { return m_selection; }
    if (m_pendingAzimuth >= 0.0) { return m_pendingAzimuth; }
    return m_target;
}

bool RotorDialWidget::route(double* travelDeg) const
{
    const double aimed = aimedAzimuth();
    *travelDeg = 0.0;
    if (aimed < 0.0 || m_actual < 0.0) { return false; }
    // The Core's predicted route for its own target.
    if (m_selecting != Selecting::Azimuth && m_pendingAzimuth < 0.0) {
        *travelDeg = m_travel;
        return m_routeKnown;
    }
    // A drag the Core has not seen yet: the same planner, from where the
    // Core says the rotor is on its span.
    const auto stop = static_cast<RotorRoute::EndStop>(m_endStop);
    const RotorRoute::Move move = stop == RotorRoute::EndStop::None
        ? RotorRoute::planFree(m_actual, aimed)
        : RotorRoute::planOnSpan(m_spanDeg, aimed, stop, m_rangeDeg);
    *travelDeg = move.travelDeg;
    return move.routeKnown;
}

QPointF RotorDialWidget::pointForBearing(double deg) const
{
    if (m_shape == Shape::Tape) {
        const double pad = 14.0;
        const double bw = width() - 2.0 * pad;
        const double mid = pad + bw / 2.0;
        const double ppd = bw / kTapeSpanDeg;
        const double from = m_actual >= 0.0 ? m_actual : 0.0;
        const double off = std::fmod(deg - from + 540.0, 360.0) - 180.0;
        return {mid + off * ppd, height() * 0.5};
    }
    const QPointF c = roseCentre();
    const double r = roseRadius() * 0.6;
    const double a = bearingToRadians(deg);
    return {c.x() + r * std::cos(a), c.y() - r * std::sin(a)};
}

QPointF RotorDialWidget::pointForElevation(double deg) const
{
    const QPointF e = gaugeCorner();
    const double r = gaugeRadius() * 0.6;
    const double a = qDegreesToRadians(deg);
    return {e.x() + r * std::cos(a), e.y() - r * std::sin(a)};
}

// From Longpath src/gui/widgets/RotorDialWidget.cpp:373-386 [@551576e]
// Bearing under the cursor, or a negative value inside the hub's dead
// zone (where the angle is meaningless and a stray pixel would swing
// the target wildly).
double RotorDialWidget::bearingAt(const QPointF& pos) const
{
    if (m_shape == Shape::Tape) { return bearingAtTape(pos); }
    const QPointF p = pos - roseCentre();
    // The dead zone scales with the rose. A fixed 8 px hub was most of
    // a compass-only dial, so on a small one the middle third of the
    // face quietly ignored clicks.
    const double dead = std::max(5.0, roseRadius() * 0.16);
    if (std::hypot(p.x(), p.y()) < dead) { return -1.0; }
    return wrap360(qRadiansToDegrees(std::atan2(p.x(), -p.y())));
}

double RotorDialWidget::elevationAt(const QPointF& pos) const
{
    if (!showsElevation()) { return -1.0; }
    const QPointF p = pos - gaugeCorner();
    const double reach = gaugeRadius() * 1.1;
    const double dist = std::hypot(p.x(), p.y());
    // Inside the quarter (right of and above the corner), clear of the
    // hub where the angle swings wildly.
    if (p.x() < -4.0 || p.y() > 4.0 || dist > reach
        || dist < std::max(5.0, gaugeRadius() * 0.16)) {
        return -1.0;
    }
    const double deg = qRadiansToDegrees(std::atan2(-p.y(), p.x()));
    return std::clamp(deg, 0.0, kMaxElevationDeg);
}

// Desktop touch rule (JJ, 2026-10-07): a press and a drag aim; nothing is
// sent until the mouse is let go, and then one turn.
void RotorDialWidget::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() != Qt::LeftButton) { QWidget::mousePressEvent(ev); return; }
    const double el = elevationAt(ev->position());
    if (el >= 0.0) {
        m_selecting = Selecting::Elevation;
        m_selection = el;
    } else {
        // The rose is aimed only from inside its rim (and the hub's dead
        // zone is no aim at all); the tape anywhere along it.
        const double deg = bearingAt(ev->position());
        const bool onRose = m_shape == Shape::Tape
            || std::hypot(ev->position().x() - roseCentre().x(),
                          ev->position().y() - roseCentre().y()) <= roseRadius() * 1.08;
        if (deg < 0.0 || !onRose) { QWidget::mousePressEvent(ev); return; }
        m_selecting = Selecting::Azimuth;
        m_selection = deg;
    }
    ev->accept();
    emit selectionChanged();
    update();
}

void RotorDialWidget::mouseMoveEvent(QMouseEvent* ev)
{
    if (m_selecting == Selecting::None) { QWidget::mouseMoveEvent(ev); return; }
    if (m_selecting == Selecting::Elevation) {
        const QPointF p = ev->position() - gaugeCorner();
        m_selection = std::clamp(qRadiansToDegrees(std::atan2(-p.y(), p.x())),
                                 0.0, kMaxElevationDeg);
    } else {
        const double deg = bearingAt(ev->position());
        if (deg >= 0.0) { m_selection = deg; }
    }
    ev->accept();
    emit selectionChanged();
    update();
}

void RotorDialWidget::mouseReleaseEvent(QMouseEvent* ev)
{
    if (ev->button() != Qt::LeftButton || m_selecting == Selecting::None) {
        QWidget::mouseReleaseEvent(ev);
        return;
    }
    const Selecting was = m_selecting;
    const double deg = std::round(m_selection);
    m_selecting = Selecting::None;
    m_selection = -1.0;
    if (was == Selecting::Elevation) {
        m_pendingElevation = deg;
    } else {
        m_pendingAzimuth = wrap360(deg);
    }
    m_pendingTimer.start(kPendingHoldMs);
    ev->accept();
    emit selectionChanged();
    update();
    if (was == Selecting::Elevation) {
        emit elevationReleased(deg);
    } else {
        emit targetReleased(wrap360(deg));
    }
}

void RotorDialWidget::endSelection()
{
    if (m_selecting == Selecting::None) { return; }
    m_selecting = Selecting::None;
    m_selection = -1.0;
    emit selectionChanged();
    update();
}

void RotorDialWidget::changeEvent(QEvent* ev)
{
    // Disabled mid-drag (the rotor went away): the drag ends and sends
    // nothing.
    if (ev->type() == QEvent::EnabledChange && !isEnabled()) {
        endSelection();
        clearSelection();
    }
    QWidget::changeEvent(ev);
}

// From Longpath src/gui/widgets/RotorDialWidget.cpp:438-450 [@551576e]
// ── Form waehlen ─────────────────────────────────────────────────────
//
// Gemerkt, weil es eine Entscheidung ist und keine Geste: wer auf das
// Band umstellt, tut das nicht fuer eine Sitzung.
void RotorDialWidget::setShape(Shape s)
{
    if (m_shape == s) { return; }
    m_shape = s;
    AppSettings::instance().setValue(
        QString::fromLatin1(kShapeSettingsKey),
        s == Shape::Tape ? QStringLiteral("Tape") : QStringLiteral("Rose"));
    updateGeometry();
    update();
}

// From Longpath src/gui/widgets/RotorDialWidget.cpp:452-589 [@551576e]
// ── Das Peilband ─────────────────────────────────────────────────────
//
// Ein Ausschnitt von 240 Grad, die Antenne fest in der Mitte. Es liest
// sich wie ein Massband, das unter einem festen Zeiger durchlaeuft —
// beim Drehen wandert die Skala, nicht die Marke.
//
// Der Gewinn ist die Flaeche: das Band nutzt die volle Breite, die ein
// Kreis nicht fuellen kann. Der Preis ist das Rundherum — was hinter
// der Antenne liegt, steht ausserhalb des Ausschnitts. Deshalb eine
// Wahl und keine Ablösung.
void RotorDialWidget::paintTape(QPainter& p)
{
    const double w = width();
    const double h = height();
    const double pad = 14.0;
    const double bx = pad, bw = w - 2.0 * pad;
    if (bw < 80.0) { return; }

    const double mid = bx + bw / 2.0;
    const double ppd = bw / kTapeSpanDeg;          // Punkte je Grad
    const double from = m_actual >= 0.0 ? m_actual : 0.0;

    // ── Die Teilung waechst mit der Hoehe ────────────────────────────
    //
    // Erst standen hier feste Laengen (16 / 11 / 6 Punkte). In einer
    // Flaeche von 1170 x 330 klebte das ganze Band dann auf einem
    // duennen Streifen in der Mitte, und ringsum war Platz, den es
    // nicht nutzte — genau der Vorwurf, den das Band eigentlich
    // aufloesen soll.
    //
    // `unit` ist die Laenge des laengsten Strichs; alles andere haengt
    // daran. Nach unten begrenzt, damit es in einem flachen Streifen
    // nicht verschwindet, nach oben, damit es in einem hohen Fenster
    // nicht zum Zaun wird.
    const double unit = qBound(14.0, h * 0.22, 64.0);
    // NereusSDR: the readout is the applet's, so the tape sits a little
    // higher than Longpath's (h * 0.56) to leave room for its labels.
    const double yb   = h * 0.50 + unit * 0.25;

    // NereusSDR: one expression; Longpath's lambda had two equal branches.
    auto px = [&](double deg) {
        return mid + (std::fmod(deg - from + 540.0, 360.0) - 180.0) * ppd;
    };

    const State st = state();
    const bool known = m_actual >= 0.0;
    const bool stale = known && !m_fresh;

    // Keule als warmes Feld um die Mitte
    if (known) {
        QColor wedge = stale ? kMuted : kActual;
        wedge.setAlpha(31);
        p.fillRect(QRectF(px(from - kBeamWidthDeg / 2.0), yb - unit * 1.15,
                          kBeamWidthDeg * ppd, unit * 1.15), wedge);
    }

    // Teilung: 5 Grad fein, 30 mittel, 90 lang
    QFont f = monoFont(10);
    for (int d = 0; d < 360; d += 5) {
        const double x = px(d);
        if (x < bx - 6.0 || x > bx + bw + 6.0) { continue; }
        const bool cardinal = (d % 90) == 0;
        const bool major    = (d % 30) == 0;
        const double len = unit * (cardinal ? 1.0 : major ? 0.68 : 0.34);
        p.setPen(QPen(cardinal ? kCardinal : kRing,
                      cardinal ? 1.5 : major ? 1.0 : 0.6));
        p.drawLine(QPointF(x, yb - len), QPointF(x, yb));
        if (!major) { continue; }
        f.setPixelSize(qBound(9, int(unit * 0.30), 15)
                       + (cardinal ? 2 : 0));
        f.setBold(cardinal);
        p.setFont(f);
        // Die vier Himmelsrichtungen in der Textfarbe: sie sind die
        // Orientierung, nicht Beiwerk. Die Zahlen dazwischen bleiben
        // leiser.
        p.setPen(cardinal ? kTapeCardinalText : kRing);
        const QString lab = (d == 0)   ? QStringLiteral("N")
                          : (d == 90)  ? QStringLiteral("E")
                          : (d == 180) ? QStringLiteral("S")
                          : (d == 270) ? QStringLiteral("W")
                                       : QString::number(d);
        p.drawText(QPointF(x - QFontMetrics(f).horizontalAdvance(lab) / 2.0,
                           yb + QFontMetrics(f).ascent() + 5.0), lab);
    }

    // Ziel
    const double aimed = st == State::OnTarget && aimedAzimuth() < 0.0 ? m_arrivedAt
                                                                         : aimedAzimuth();
    if (aimed >= 0.0) {
        const double tx = px(aimed);
        if (tx > bx - 8.0 && tx < bx + bw + 8.0) {
            QPen pen(st == State::OnTarget ? kArrived : kTarget, 1.6);
            pen.setStyle(Qt::DashLine);
            pen.setDashPattern({2.6, 2.2});
            p.setPen(pen);
            p.drawLine(QPointF(tx, yb - unit * 1.15), QPointF(tx, yb));
            QPolygonF mark;
            mark << QPointF(tx, yb) << QPointF(tx - 5.0, yb + 9.0)
                 << QPointF(tx + 5.0, yb + 9.0);
            p.setPen(Qt::NoPen);
            p.setBrush(st == State::OnTarget ? kArrived : kTarget);
            p.drawPolygon(mark);
            p.setBrush(Qt::NoBrush);
        }
    }

    // Die feste Marke in der Mitte: SIE ist die Antenne.
    if (!known) { return; }
    const QColor actualCol = stale                       ? kMuted
                           : (st == State::Turning)      ? kTurning
                           : (st == State::OnTarget)     ? kArrived
                                                         : kActual;
    const double headY = yb - unit * 1.45;
    p.setPen(QPen(actualCol, 2.4));
    p.drawLine(QPointF(mid, headY), QPointF(mid, yb + 4.0));
    QPolygonF head;
    head << QPointF(mid, headY)
         << QPointF(mid - 7.0, headY - 10.0)
         << QPointF(mid + 7.0, headY - 10.0);
    p.setPen(Qt::NoPen);
    p.setBrush(actualCol);
    p.drawPolygon(head);
    p.setBrush(Qt::NoBrush);
}

// From Longpath src/gui/widgets/RotorDialWidget.cpp:591-599 [@551576e]
double RotorDialWidget::bearingAtTape(const QPointF& pos) const
{
    const double pad = 14.0;
    const double bw = width() - 2.0 * pad;
    if (bw < 80.0) { return -1.0; }
    const double mid = pad + bw / 2.0;
    const double ppd = bw / kTapeSpanDeg;
    const double from = m_actual >= 0.0 ? m_actual : 0.0;
    return wrap360(from + (pos.x() - mid) / ppd);
}

// From Longpath src/gui/widgets/RotorDialWidget.cpp:601-606 [@551576e]
void RotorDialWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (!isEnabled()) { p.setOpacity(kDisabledOpacity); }
    paintFace(p);
}

// From Longpath src/gui/widgets/RotorDialWidget.cpp:624-951 [@551576e]
// NereusSDR: no background fill (the applet's shows through, as in the
// mockup), no accent wash and no readout; the elevation gauge after the
// rose on an az/el rotor.
void RotorDialWidget::paintFace(QPainter& p)
{
    if (m_shape == Shape::Tape) {
        paintTape(p);
        return;
    }

    const State st = state();
    const bool known = m_actual >= 0.0;
    const bool stale = known && !m_fresh;

    // ── Und eine Lampe hinter der Rose ───────────────────────────────
    //
    // Dasselbe Mittel wie bei den Zeigerinstrumenten (siehe
    // NeedleInstrument::paintEvent): ein sehr schwacher Verlauf aus der
    // Mitte, in der Messfarbe. Er beleuchtet die ganze Scheibe, statt
    // nur den Zeiger heller zu machen — „von hinten leicht
    // beleuchten", wie es hiess.
    //
    // Auch im durchsichtigen Fall: gerade dort, ueber dem Spektrum,
    // braucht die Rose etwas, das sie vom Untergrund abhebt.
    {
        const QPointF lc = roseCentre();
        const double  lr = roseRadius();
        QRadialGradient lamp(lc, lr * 1.05);
        QColor warm(kActual);
        warm.setAlphaF(0.12);
        lamp.setColorAt(0.0, warm);
        warm.setAlphaF(0.0);
        lamp.setColorAt(1.0, warm);
        p.setPen(Qt::NoPen);
        p.setBrush(lamp);
        p.drawEllipse(lc, lr * 1.05, lr * 1.05);
        p.setBrush(Qt::NoBrush);
    }

    const QPointF c = roseCentre();
    const double  r = roseRadius();

    // Rings. The outer one turns amber and dashed while the needle is
    // invented — a mark that costs no space, so it survives at any size
    // the dial can be dragged to. The words below may not fit; this
    // always does.
    p.setPen(QPen(kRing, 0.9));
    p.drawEllipse(c, r, r);
    p.setPen(QPen(kRingInner, 0.9));
    p.drawEllipse(c, r * 0.83, r * 0.83);

    // ── Die Teilung: drei Stufen statt einer ─────────────────────────
    //
    // Hier stand eine Teilung alle 30° und sonst nichts. Der Betreiber
    // hat am 2026-08-20 um „neue, passende und aufwendigere Grafiken"
    // gebeten, und eine Windrose mit zwoelf Strichen ist fuer ein
    // Instrument, an dem man Grade ablesen soll, zu grob.
    //
    // Drei Stufen, wie an jedem Kompass: alle 10° ein kurzer Strich,
    // alle 30° ein laengerer, auf den Haupthimmelsrichtungen der
    // laengste. Die Abstufung macht das Zaehlen ueberfluessig — man
    // sieht die Zehner, ohne sie abzuzaehlen.
    for (int deg = 0; deg < 360; deg += 10) {
        const double a = bearingToRadians(deg);
        const bool cardinal = (deg % 90) == 0;
        const bool major    = (deg % 30) == 0;
        const double r0 = r * (cardinal ? 0.86 : major ? 0.90 : 0.94);
        p.setPen(QPen(cardinal ? kMuted : kRing,
                      cardinal ? 1.5 : major ? 1.1 : 0.6));
        p.drawLine(QPointF(c.x() + r0 * std::cos(a), c.y() - r0 * std::sin(a)),
                   QPointF(c.x() + r  * std::cos(a), c.y() - r  * std::sin(a)));
    }

    // ── Gradzahlen alle 30°, wenn Platz ist ──────────────────────────
    //
    // Nur ab einem Radius, bei dem sie sich nicht beruehren, und ohne
    // die vier Himmelsrichtungen: dort stehen schon N/E/S/W, und eine
    // „0" unter dem N waere doppelt gemoppelt.
    if (r > 84.0) {
        QFont df = monoFont(9);
        p.setFont(df);
        const QFontMetrics dfm(df);
        QColor degInk(kRing);
        degInk.setAlpha(150);
        p.setPen(degInk);
        for (int deg = 30; deg < 360; deg += 30) {
            if (deg % 90 == 0) { continue; }
            const double a  = bearingToRadians(deg);
            const double rr = r * 0.755;
            const QString t = QStringLiteral("%1").arg(deg);
            p.drawText(QPointF(c.x() + rr * std::cos(a)
                                   - dfm.horizontalAdvance(t) / 2.0,
                               c.y() - rr * std::sin(a) + dfm.ascent() / 2.0),
                       t);
        }
    }

    // Cardinal letters
    //
    // In derselben Schmalschrift wie die Teilung der uebrigen
    // Instrumente (InstrumentPainter benutzt Style::monoFont fuer die
    // Skalenbeschriftung). Eine Windrose in der Fliesstextschrift neben
    // Zifferblaettern in Schmalschrift war der zweite Teil dessen, was
    // der Betreiber am 2026-08-20 als „nicht im stil der anderen
    // grafiken" gesehen hat — der erste war die Farbe des Zeigers.
    QFont cf = monoFont(11);
    p.setFont(cf);
    p.setPen(kCardinal);
    const QFontMetrics cfm(cf);
    const struct { const char* s; int deg; } kCards[] = {
        {"N", 0}, {"E", 90}, {"S", 180}, {"W", 270}};
    // ── Die Himmelsrichtungen INNEN, im Ring der Gradzahlen ─────────
    //
    // Sie standen AUSSERHALB der Rose (r + 11). Damit lag das „S"
    // unter dem unteren Ringrand — genau dort, wo die Ablesung
    // beginnt, und auf dem Bild vom 2026-08-20 steckte es in der
    // „120°".
    //
    // Auf demselben Radius wie die Gradzahlen ergeben die vier
    // Buchstaben und die acht Zahlen EINEN Beschriftungsring statt
    // zweier, die Rose gewinnt aussen 11 px, und unter ihr bleibt die
    // Flaeche frei fuer das, was dort hingehoert.
    for (const auto& card : kCards) {
        const double a = bearingToRadians(card.deg);
        const double rr = r * 0.755;
        const QString s = QString::fromLatin1(card.s);
        p.drawText(QPointF(c.x() + rr * std::cos(a) - cfm.horizontalAdvance(s) / 2.0,
                           c.y() - rr * std::sin(a) + cfm.ascent() / 2.0),
                   s);
    }

    // NereusSDR: the end stop on the rim, a short bar across it where the
    // rotor cannot turn through (Longpath setEndStop, drawn).
    if (m_endStop != static_cast<int>(RotorRoute::EndStop::None)) {
        const double stopDeg =
            RotorRoute::stopCompassDeg(static_cast<RotorRoute::EndStop>(m_endStop));
        const double a = bearingToRadians(stopDeg);
        const QPointF dir(std::cos(a), -std::sin(a));
        p.setPen(QPen(kMuted, 3.0, Qt::SolidLine, Qt::FlatCap));
        p.drawLine(QPointF(c.x() + r * 0.97 * dir.x(), c.y() + r * 0.97 * dir.y()),
                   QPointF(c.x() + r * 1.07 * dir.x(), c.y() + r * 1.07 * dir.y()));
    }

    const double aimed = aimedAzimuth();

    // Travel sector: from actual towards target along the legal path.
    // NereusSDR: the Core's predicted route (the long way round when the
    // stop forces it); none while the route is not known.
    double travel = 0.0;
    if (aimed >= 0.0 && st != State::OnTarget && route(&travel) && travel != 0.0) {
        const QRectF box(c.x() - r, c.y() - r, r * 2, r * 2);
        // Qt angles: 0 at 3 o'clock, counter-clockwise positive.
        const int startQt = static_cast<int>((90.0 - m_actual) * 16);
        const int spanQt  = static_cast<int>(-travel * 16);
        QColor sector = (st == State::Turning) ? kTurning : kTarget;
        sector.setAlpha(st == State::Turning ? 30 : 26);
        p.setPen(Qt::NoPen);
        p.setBrush(sector);
        p.drawPie(box, startQt, spanQt);
        p.setBrush(Qt::NoBrush);
    }

    // Beam-width wedge around the actual heading
    if (known) {
        const QRectF box(c.x() - r * 0.95, c.y() - r * 0.95, r * 1.9, r * 1.9);
        // ── Die Keule sitzt auf der Antennenrichtung ────────────────
        //
        // Hier stand `- m_beamWidth / 2.0`. Qt zaehlt von 3 Uhr gegen
        // den Uhrzeigersinn, eine Peilung von Nord im Uhrzeigersinn;
        // die Umrechnung ist Qt = 90 - Peilung. Der Sektor soll um
        // diesen Wert HERUM liegen, also bei Qt+Haelfte anfangen und
        // mit negativer Spanne darueber hinweglaufen.
        //
        // Mit dem Minus fing er eine halbe Keulenbreite zu frueh an und
        // lief eine halbe zu frueh aus — die ganze Keule stand um ihre
        // eigene Breite neben dem Zeiger. Beim Rendern am 2026-08-20
        // sofort zu sehen: Zeiger auf 45°, Keule nach Osten.
        const int startQt = static_cast<int>((90.0 - m_actual + kBeamWidthDeg / 2.0) * 16);
        const int spanQt  = static_cast<int>(-kBeamWidthDeg * 16);
        QColor wedge = stale ? kMuted : kActual;
        wedge.setAlpha(16);
        p.setPen(Qt::NoPen);
        p.setBrush(wedge);
        p.drawPie(box, startQt, spanQt);
        p.setBrush(Qt::NoBrush);
    }

    // ── Der Zeiger ───────────────────────────────────────────────────
    //
    // Bisher ein Strich von der Mitte nach aussen. Ein Instrumenten-
    // zeiger ist etwas anderes: er verjuengt sich zur Spitze, und er
    // hat hinter der Achse ein kurzes Gegengewicht. Beides hat einen
    // Zweck, nicht nur ein Aussehen — die Verjuengung sagt, welches
    // Ende die Ablesung ist, und das Gegengewicht macht die Drehachse
    // als Achse kenntlich statt als Anfangspunkt eines Strichs.
    //
    // Der gestrichelte Zielzeiger bleibt ein Strich: er ist eine
    // Vorgabe, kein Messwerk, und soll auch so aussehen.
    auto drawNeedle = [&](double deg, const QColor& col, double len,
                          bool dashed, double width) {
        const double a = bearingToRadians(deg);
        const QPointF dir(std::cos(a), -std::sin(a));
        const QPointF nrm(-dir.y(), dir.x());
        const QPointF tip(c.x() + r * len * dir.x(),
                          c.y() + r * len * dir.y());

        if (dashed) {
            QColor halo = col;
            halo.setAlpha(70);
            p.setPen(QPen(halo, width + 2.6, Qt::SolidLine, Qt::RoundCap));
            p.drawLine(c, tip);
            QPen pen(col, width, Qt::DashLine, Qt::RoundCap);
            pen.setDashPattern({2.2, 1.6});
            p.setPen(pen);
            p.drawLine(c, tip);
            return;
        }

        const double halfW = width * 0.9;
        const double tailL = r * 0.16;
        const QPointF tail(c.x() - tailL * dir.x(), c.y() - tailL * dir.y());

        QPolygonF body;
        body << tip
             << QPointF(c.x() + nrm.x() * halfW, c.y() + nrm.y() * halfW)
             << QPointF(tail.x() + nrm.x() * halfW * 0.7,
                        tail.y() + nrm.y() * halfW * 0.7)
             << QPointF(tail.x() - nrm.x() * halfW * 0.7,
                        tail.y() - nrm.y() * halfW * 0.7)
             << QPointF(c.x() - nrm.x() * halfW, c.y() - nrm.y() * halfW);

        QColor halo = col;
        halo.setAlpha(60);
        // Vierter Parameter ist die KAPPE, nicht die Ecke — der
        // Eckenstil kommt danach.
        QPen haloPen(halo, 3.0, Qt::SolidLine, Qt::RoundCap);
        haloPen.setJoinStyle(Qt::RoundJoin);
        p.setPen(haloPen);
        p.setBrush(Qt::NoBrush);
        p.drawPolygon(body);

        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawPolygon(body);
        p.setBrush(Qt::NoBrush);
    };

    // ── Die Zielmarke am Rand ────────────────────────────────────────
    //
    // Ein gestrichelter Strich sagt die Richtung, aber nicht genau, wo
    // sie den Rand trifft. Ein kleines Dreieck auf dem Ring tut das —
    // und bleibt lesbar, wenn Zeiger und Ziel dicht beieinander
    // stehen, wo sich zwei Striche sonst zu einem verwischen.
    const double rimDeg = aimed >= 0.0 ? aimed : m_arrivedAt;
    if (rimDeg >= 0.0) {
        const double a = bearingToRadians(rimDeg);
        const QPointF dir(std::cos(a), -std::sin(a));
        const QPointF nrm(-dir.y(), dir.x());
        const QPointF onRim(c.x() + r * dir.x(), c.y() + r * dir.y());
        const QPointF inner(c.x() + (r - 9.0) * dir.x(),
                            c.y() + (r - 9.0) * dir.y());
        QPolygonF mark;
        mark << onRim
             << QPointF(inner.x() + nrm.x() * 4.5, inner.y() + nrm.y() * 4.5)
             << QPointF(inner.x() - nrm.x() * 4.5, inner.y() - nrm.y() * 4.5);
        p.setPen(Qt::NoPen);
        p.setBrush(st == State::OnTarget ? kArrived : kTarget);
        p.drawPolygon(mark);
        p.setBrush(Qt::NoBrush);
    }

    // Target first so the actual needle reads on top of it.
    if (aimed >= 0.0 && st != State::OnTarget) {
        drawNeedle(aimed, kTarget, 0.90, /*dashed=*/true, 2.4);
    }

    // ── Der Zeiger spricht dieselbe Sprache wie Nabe und Ablesung ────
    //
    // Hier stand fuer BEIDE Zustaende — dreht und am Ziel — dasselbe
    // Rot, waehrend die Nabe darunter schon amber (dreht) und gruen
    // (am Ziel) faerbte und die Ablesung ebenso. Drei Stellen
    // desselben Instruments sagten damit zwei verschiedene Dinge, und
    // das Rot behauptete oben Gefahr, wo unten „angekommen" stand.
    //
    // Jetzt einheitlich: bernsteinfarben in Ruhe (gemessen), waehrend
    // der Fahrt dasselbe Amber wie die Nabe, am Ziel gruen.
    // NereusSDR: a stale heading (no reply for 1500 ms) is muted, never
    // drawn as live; an unknown one draws no needle at all, since a needle
    // at north would say the antenna points north.
    const QColor actualCol = stale                       ? kMuted
                           : (st == State::Turning)      ? kTurning
                           : (st == State::OnTarget)     ? kArrived
                                                         : kActual;
    if (known) {
        drawNeedle(m_actual, actualCol, 0.90, /*dashed=*/false, 2.6);
    }

    // Hub
    const QColor hubCol = !known || stale            ? kMuted
                        : (st == State::Turning)     ? kTurning
                        : (st == State::OnTarget)    ? kArrived
                                                     : kActual;
    QRadialGradient hub(c, 9);
    hub.setColorAt(0.0, hubCol);
    QColor hubEdge = hubCol;
    hubEdge.setAlpha(0);
    hub.setColorAt(1.0, hubEdge);
    p.setPen(Qt::NoPen);
    p.setBrush(hub);
    p.drawEllipse(c, 9, 9);
    p.setBrush(hubCol);
    p.drawEllipse(c, 3.6, 3.6);
    p.setBrush(Qt::NoBrush);

    if (showsElevation()) {
        paintElevation(p);
    }
}

// NereusSDR-original: the elevation quarter gauge beside the rose on an
// az/el rotor, in the rose's language (rings, three-step ticks, the same
// tapered needle pivoting at the corner, the dashed target, the travel
// pie), as the rotor mockup draws it.
void RotorDialWidget::paintElevation(QPainter& p)
{
    const QPointF e = gaugeCorner();
    const double er = gaugeRadius();
    const bool stale = m_elevation >= 0.0 && !m_fresh;
    auto at = [&](double rr, double deg) {
        const double a = qDegreesToRadians(deg);
        return QPointF(e.x() + rr * std::cos(a), e.y() - rr * std::sin(a));
    };
    const QRectF box(e.x() - er, e.y() - er, er * 2, er * 2);

    // A faint glow over the quarter, the arcs and the two axes.
    {
        QColor glow(kActual);
        glow.setAlphaF(0.05);
        p.setPen(Qt::NoPen);
        p.setBrush(glow);
        p.drawPie(box, 0, 90 * 16);
        p.setBrush(Qt::NoBrush);
    }
    p.setPen(QPen(kRing, 0.9));
    p.drawArc(box, 0, 90 * 16);
    p.setPen(QPen(kRingInner, 0.9));
    p.drawArc(QRectF(e.x() - er * 0.83, e.y() - er * 0.83, er * 1.66, er * 1.66), 0, 90 * 16);
    p.drawLine(e, at(er, 0.0));
    p.drawLine(e, at(er, 90.0));

    // Ticks every 5 degrees: 30s longest, 10s longer.
    for (int d = 0; d <= 90; d += 5) {
        const bool major = d % 30 == 0;
        const bool mid = d % 10 == 0;
        p.setPen(QPen(major ? kMuted : kRing, major ? 1.4 : mid ? 1.0 : 0.6));
        p.drawLine(at(er * (major ? 0.86 : mid ? 0.90 : 0.94), d), at(er, d));
    }
    QFont lf = monoFont(8);
    p.setFont(lf);
    const QFontMetrics lfm(lf);
    QColor ink(kRing);
    ink.setAlpha(150);
    p.setPen(ink);
    for (int d : {30, 60}) {
        const QString t = QString::number(d);
        const QPointF pt = at(er * 0.72, d);
        p.drawText(QPointF(pt.x() - lfm.horizontalAdvance(t) / 2.0, pt.y() + lfm.ascent() / 2.0), t);
    }
    p.setPen(kRing);
    p.drawText(QPointF(e.x() + er - lfm.horizontalAdvance(QStringLiteral("0")) / 2.0,
                       e.y() + lfm.ascent() + 3.0), QStringLiteral("0"));
    p.drawText(QPointF(e.x() - lfm.horizontalAdvance(QStringLiteral("90")) / 2.0,
                       e.y() - er - 4.0), QStringLiteral("90"));

    // The target: a held drag, one just sent, or the Core's.
    const double target = m_selecting == Selecting::Elevation ? m_selection
                        : m_pendingElevation >= 0.0         ? m_pendingElevation
                                                            : m_targetElevation;
    const bool selecting = m_selecting == Selecting::Elevation || m_pendingElevation >= 0.0;
    if (target >= 0.0) {
        if (m_elevation >= 0.0) {
            const double lo = std::min(m_elevation, target);
            const double hi = std::max(m_elevation, target);
            QColor pie = selecting ? kTarget : kTurning;
            pie.setAlpha(selecting ? 26 : 30);
            p.setPen(Qt::NoPen);
            p.setBrush(pie);
            p.drawPie(box, static_cast<int>(lo * 16), static_cast<int>((hi - lo) * 16));
            p.setBrush(Qt::NoBrush);
        }
        QPen pen(kTarget, 1.6, Qt::DashLine, Qt::RoundCap);
        pen.setDashPattern({3.1, 1.9});
        p.setPen(pen);
        p.drawLine(e, at(er * 0.97, target));
    }

    // The needle: the rose's, pivoting at the corner.
    if (m_elevation >= 0.0) {
        const QColor col = stale ? kMuted : kActual;
        const double a = qDegreesToRadians(m_elevation);
        const QPointF dir(std::cos(a), -std::sin(a));
        const QPointF nrm(-dir.y(), dir.x());
        const double halfW = 2.6 * 0.9;
        const double tailL = er * 0.10;
        const QPointF tip(e.x() + er * 0.9 * dir.x(), e.y() + er * 0.9 * dir.y());
        const QPointF tail(e.x() - tailL * dir.x(), e.y() - tailL * dir.y());
        QPolygonF body;
        body << tip
             << QPointF(e.x() + nrm.x() * halfW, e.y() + nrm.y() * halfW)
             << QPointF(tail.x() + nrm.x() * halfW * 0.7, tail.y() + nrm.y() * halfW * 0.7)
             << QPointF(tail.x() - nrm.x() * halfW * 0.7, tail.y() - nrm.y() * halfW * 0.7)
             << QPointF(e.x() - nrm.x() * halfW, e.y() - nrm.y() * halfW);
        QColor halo = col;
        halo.setAlpha(60);
        QPen haloPen(halo, 3.0, Qt::SolidLine, Qt::RoundCap);
        haloPen.setJoinStyle(Qt::RoundJoin);
        p.setPen(haloPen);
        p.drawPolygon(body);
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawPolygon(body);
        QColor hubGlow = col;
        hubGlow.setAlphaF(0.25);
        p.setBrush(hubGlow);
        p.drawEllipse(e, 7.0, 7.0);
        p.setBrush(col);
        p.drawEllipse(e, 3.6, 3.6);
        p.setBrush(Qt::NoBrush);

        QFont tf = monoFont(9);
        p.setFont(tf);
        p.setPen(kMuted);
        const QString t = QStringLiteral("EL %1°").arg(qRound(m_elevation));
        p.drawText(QPointF(e.x() + er * 0.5 - QFontMetrics(tf).horizontalAdvance(t) / 2.0,
                           e.y() - 6.0), t);
    }
}

} // namespace NereusSDR
