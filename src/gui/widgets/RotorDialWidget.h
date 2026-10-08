// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/widgets/RotorDialWidget.h  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   src/gui/widgets/RotorDialWidget.h [@551576e], original header from
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
//               Claude Code. Rotor control plan, Task 6. Namespace
//               Longpath becomes NereusSDR. The dial reads the Core's
//               rotor object (RotorLink::RotorModel) instead of being
//               told each value, and its travel sector follows the
//               route the Core predicts (travelDeg, routeKnown); while
//               a drag is held it plans the route itself with the same
//               planner (core/RotorRoute.h). Desktop touch rule (JJ,
//               2026-10-07): a press and drag aim, letting go sends one
//               turn (targetReleased); Longpath's click-to-aim and
//               double-click-to-turn are not taken. The end stop is
//               marked on the rim. On an az/el rotor an elevation
//               quarter gauge sits beside the rose (NereusSDR's own;
//               Longpath shows elevation only as a corner readout). A
//               stale heading is drawn muted; an unknown one draws no
//               needle. The readout lives in the Rotor applet, not on
//               the face; the simulated needle, the transparent image
//               for the panadapter and the landscape layout are not
//               taken. NereusSDR's palette (the rotor mockup's colours)
//               replaces Longpath's; menu words are English.
// =================================================================
//
// --- From RotorDialWidget.h ---
//
// =================================================================
// src/gui/widgets/RotorDialWidget.h  (Longpath)
// =================================================================
//
// Longpath-original. Thetis has no rotator control. The project's
// existing `gui/meters/RotatorItem` is a meter-container *item* that
// draws one heading inside the MeterWidget scene graph; this is a
// standalone two-needle instrument (target + actual) with its own
// interaction, so it is a sibling rather than a reuse.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//                 Step 1 of the QRZ logbook work: display only, no
//                 rotator protocol behind it yet.
//   2026-08-10 — Elevation readout: shown in the top-right corner once
//                 a rotator has reported one. An azimuth-only rotator
//                 never triggers it, so nothing changes for the common
//                 case. AI-assisted via Anthropic Claude (Cowork),
//                 operator Martin Fischer.
// =================================================================

#pragma once

#include <QElapsedTimer>
#include <QPointF>
#include <QPointer>
#include <QTimer>
#include <QWidget>

class QContextMenuEvent;
class QMouseEvent;
class QPainter;
class QPaintEvent;

namespace NereusSDR {

namespace RotorLink { class RotorModel; }

// From Longpath src/gui/widgets/RotorDialWidget.h:34-46 [@551576e]
// Compass rose showing where the antenna IS and where it SHOULD point.
//
// Two needles:
//   actual  — white, the rotator's reported heading
//   target  — accent, dashed, the bearing to the worked station
// plus the sector between them along the direction the rotator will
// actually travel.
//
// The widget holds no rotator connection. It reports what the operator
// asked for (rotateRequested / stopRequested / targetPicked) and shows
// what it is told (setActualBearing / setState). That split keeps the
// dial usable with a simulated rotator, a network one, or none at all.
class RotorDialWidget : public QWidget {
    Q_OBJECT
public:
    // From Longpath src/gui/widgets/RotorDialWidget.h:49-55 [@551576e]
    enum class State {
        Idle,      // no target — only the actual needle
        Targeted,  // target set, rotator not moving
        Turning,   // rotator in motion
        OnTarget,  // within the arrival tolerance
    };
    Q_ENUM(State)

    // From Longpath src/gui/widgets/RotorDialWidget.h:101-117 [@551576e]
    // ── Zwei Formen ──────────────────────────────────────────────────
    //
    // Der Betreiber, 2026-08-21, nach dem Entwurfsblatt: „beide zur
    // auswahl, standard vollkreis."
    //
    // Der Grund steht im Entwurf (Longpath-Kompass-Entwurf.pdf): ein
    // Vollkreis ist so breit wie hoch. In einer Flaeche von 1180 x 330
    // begrenzt die HOEHE den Radius auf rund 150 — er nutzt damit 300
    // von 1180 Punkten Breite, und die restlichen 880 kann er nicht
    // fuellen, egal wie man rechnet. S-Meter und Stehwelle wirken
    // richtig proportioniert, weil sie Halbkreise sind.
    //
    // Das Band loest das, indem es die Rundform aufgibt: ein
    // Ausschnitt von +-120 Grad um die Antenne, die fest in der Mitte
    // steht. Was es dafuer verliert, ist das Rundherum — deshalb eine
    // WAHL und keine Ablösung.
    enum class Shape { Rose, Tape };
    Q_ENUM(Shape)

    // The window preference that remembers the shape (AppSettings).
    static constexpr const char* kShapeSettingsKey = "Rotor/DialShape";
    // From Longpath src/core/RotctldClient.cpp:30 [@551576e], through
    // RotorConnection::kArrivedDeg: how close counts as arrived.
    static constexpr double kArrivalToleranceDeg = 1.5;
    // From Longpath src/gui/widgets/RotorDialWidget.h:235-236 [@551576e]
    /// Sichtbarer Ausschnitt des Bandes, in Grad (Vollbreite).
    // NereusSDR: 200 rather than Longpath's 240, as the rotor mockup draws it.
    static constexpr double kTapeSpanDeg = 200.0;
    // From Longpath src/gui/widgets/RotorDialWidget.h:84, 249 [@551576e]
    // Antenna beam width, drawn as a wedge around the actual heading.
    static constexpr double kBeamWidthDeg = 40.0;
    // A target sent on release is drawn as the operator left it until the
    // Core reports it (or refuses it). This design's choice.
    static constexpr int kPendingHoldMs = 5000;

    explicit RotorDialWidget(QWidget* parent = nullptr);
    ~RotorDialWidget() override;

    /// Follow `model` from now on (nullptr: show nothing known).
    void setRotorModel(RotorLink::RotorModel* model);

    void  setShape(Shape s);
    Shape shape() const noexcept { return m_shape; }

    State state() const;
    /// True while the mouse holds a drag on the dial.
    bool  selecting() const noexcept { return m_selecting != Selecting::None; }
    /// The heading (or elevation, while the elevation gauge is dragged)
    /// under the drag; -1 when nothing is being aimed.
    double selectionDeg() const;
    bool   selectingElevation() const noexcept { return m_selecting == Selecting::Elevation; }
    /// Drop a target sent on release (the Core refused it).
    void clearSelection();

    /// Where the rose or the tape puts `deg`, and where the elevation
    /// gauge puts an elevation: for a press there (tests, and the applet).
    QPointF pointForBearing(double deg) const;
    QPointF pointForElevation(double deg) const;
    bool    showsElevation() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    /// The operator let go of a drag on the rose or tape: turn here.
    void targetReleased(double azimuthDeg);
    /// The operator let go of a drag on the elevation gauge.
    void elevationReleased(double elevationDeg);
    /// The heading under a held drag moved, or the drag began or ended.
    void selectionChanged();

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;
    void contextMenuEvent(QContextMenuEvent* ev) override;
    void changeEvent(QEvent* ev) override;

private:
    enum class Selecting { None, Azimuth, Elevation };

    void refreshFromModel();
    // Screen angle for a compass bearing: 0° is up, clockwise.
    static double bearingToRadians(double deg);
    // Compass bearing under a widget-local point; negative inside the
    // hub's dead zone, where the angle is meaningless.
    double bearingAt(const QPointF& pos) const;
    double bearingAtTape(const QPointF& pos) const;
    // Elevation under a point inside the gauge's quarter; negative outside.
    double elevationAt(const QPointF& pos) const;
    // The heading the dial aims at: a held or just-sent drag first, then
    // the Core's target.
    double aimedAzimuth() const;
    // The signed route to `aimedAzimuth()` and whether it is known.
    bool   route(double* travelDeg) const;
    void   endSelection();

    // From Longpath src/gui/widgets/RotorDialWidget.h:188-198 [@551576e]
    // ── Where the rose is, in one place ──────────────────────────────
    //
    // The rose normally sits high, with the readout under it; when the
    // widget is small it takes the whole face instead. Two functions
    // need to agree about that: the one that draws it and the one that
    // works out which bearing the mouse landed on.
    //
    // They did not. paintEvent moved the centre and bearingAt() kept
    // its own copy of the old formula, so on a small dial a click aimed
    // at a heading several degrees from the one under the cursor —
    // silently, and worse the smaller it got.
    double  faceUnit()      const;
    double  faceLeft()      const;
    double  faceTop()       const;
    QPointF roseCentre()    const;
    double  roseRadius()    const;
    QPointF gaugeCorner()   const;
    double  gaugeRadius()   const;

    /// Das Band statt der Rose. Siehe Shape.
    void   paintTape(QPainter& p);
    void   paintFace(QPainter& p);
    void   paintElevation(QPainter& p);

    QPointer<RotorLink::RotorModel> m_model;
    QMetaObject::Connection m_stateConnection;
    QMetaObject::Connection m_positionConnection;

    Shape  m_shape{Shape::Rose};

    // What the rotor object says.
    double m_actual{-1.0};
    double m_elevation{-1.0};
    bool   m_elevationAxis{false};
    double m_target{-1.0};
    double m_targetElevation{-1.0};
    bool   m_moving{false};
    double m_travel{0.0};
    bool   m_routeKnown{true};
    bool   m_fresh{false};
    int    m_endStop{0};          // the wire's endStop enum
    double m_rangeDeg{360.0};
    double m_spanDeg{-1.0};

    // The last target, kept once the Core drops it on arrival so the dial
    // can show the arrival in green.
    double m_lastTarget{-1.0};
    double m_arrivedAt{-1.0};

    // The operator's drag, and a drag just sent.
    Selecting m_selecting{Selecting::None};
    double m_selection{-1.0};
    double m_pendingAzimuth{-1.0};
    double m_pendingElevation{-1.0};
    QTimer m_pendingTimer;
};

} // namespace NereusSDR
