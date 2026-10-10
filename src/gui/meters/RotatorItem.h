#pragma once

// =================================================================
// src/gui/meters/RotatorItem.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-10-08 - Rotor control plan Task 5: drag to turn and the stop
//                 circle, ported from Thetis renderRotator() and
//                 clsRotatorItem.SendRotatorMessage/MouseUp/MouseDown, sent
//                 through RotorCommandSink instead of an MMIO template; the
//                 target marker in amber; elevation fed and drawn; "Both"
//                 laid out two faces wide as in Thetis. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "MeterItem.h"
#include <QColor>
#include <QImage>
#include <QPointF>
#include <QPointer>
#include <QTimer>

namespace NereusSDR {

class RotorCommandSink;
namespace RotorLink { class RotorModel; }

// From Thetis clsRotatorItem (MeterManager.cs:15042+)
// Antenna rotator compass dial with AZ/ELE/BOTH modes.
class RotatorItem : public MeterItem {
    Q_OBJECT

public:
    enum class RotatorMode { Az, Ele, Both };

    explicit RotatorItem(QObject* parent = nullptr);

    void setMode(RotatorMode m) { m_mode = m; }
    RotatorMode mode() const { return m_mode; }

    void setShowValue(bool s) { m_showValue = s; }
    bool showValue() const { return m_showValue; }

    void setShowCardinals(bool s) { m_showCardinals = s; }
    bool showCardinals() const { return m_showCardinals; }

    void setShowBeamWidth(bool s) { m_showBeamWidth = s; }
    bool showBeamWidth() const { return m_showBeamWidth; }

    void setBeamWidth(float deg) { m_beamWidth = deg; }
    float beamWidth() const { return m_beamWidth; }

    void setBeamWidthAlpha(float a) { m_beamWidthAlpha = a; }
    float beamWidthAlpha() const { return m_beamWidthAlpha; }

    void setDarkMode(bool d) { m_darkMode = d; }
    bool darkMode() const { return m_darkMode; }
    void setPadding(float p) { m_padding = p; }
    float padding() const { return m_padding; }

    // Colors (from Thetis clsRotatorItem properties)
    void setBigBlobColour(const QColor& c) { m_bigBlobColour = c; }
    QColor bigBlobColour() const { return m_bigBlobColour; }
    void setSmallBlobColour(const QColor& c) { m_smallBlobColour = c; }
    QColor smallBlobColour() const { return m_smallBlobColour; }
    void setOuterTextColour(const QColor& c) { m_outerTextColour = c; }
    QColor outerTextColour() const { return m_outerTextColour; }
    void setArrowColour(const QColor& c) { m_arrowColour = c; }
    QColor arrowColour() const { return m_arrowColour; }
    void setBeamWidthColour(const QColor& c) { m_beamWidthColour = c; }
    QColor beamWidthColour() const { return m_beamWidthColour; }
    void setBackgroundColour(const QColor& c) { m_backgroundColour = c; }
    QColor backgroundColour() const { return m_backgroundColour; }

    // Background image (file-based, user-replaceable)
    void setBackgroundImagePath(const QString& path);
    QString backgroundImagePath() const { return m_bgImagePath; }

    // Elevation value (for BOTH mode — azimuth uses base m_value)
    // Rotor control plan Task 5: smoothed into the drawn elevation the way
    // Thetis's Update() smooths a Reading.ELE item (MeterManager.cs:16604).
    void setElevation(float ele);
    float elevation() const { return m_elevation; }

    void setValue(double v) override;

    // What the dial draws now (after smoothing).
    float displayedAzimuth() const { return m_smoothedAz; }
    float displayedElevation() const { return m_smoothedEle; }

    // ── Rotor control plan Task 5: drag to turn ──
    // The rotor this item shows and turns: `state` (RadioModel::rotorModel())
    // feeds the heading and says whether a rotor is there to turn;
    // `commands` (RadioModel) takes the turn and stop. `commands` must live
    // as long as `state` (RadioModel owns both). nullptr, nullptr detaches:
    // the dial then only displays, as before.
    void setRotor(RotorLink::RotorModel* state, RotorCommandSink* commands);
    // A rotor is connected and the commands can reach it. Without one a
    // press or drag does nothing (the dial stays, showing what it last had).
    bool rotorControllable() const;
    bool elevationControllable() const;

    // Thetis's "no angle" value (MeterManager.cs:36699).
    static constexpr float kNoAngle = -999.0f;
    // The heading a release would send now, or kNoAngle.
    float dragDegrees() const { return m_dragDegrees; }
    bool draggingElevation() const { return m_dragEle; }
    // The heading last sent, shown in amber until the arrow comes within
    // kArrivedDeg of it; kNoAngle when none.
    float targetAzimuthMarker() const { return m_targetAz; }
    float targetElevationMarker() const { return m_targetEle; }

    // Hit areas, for the window and the tests: the centre of each face and
    // its stop circle, in widget pixels.
    QPointF azimuthCentre(int widgetW, int widgetH) const;
    QPointF elevationCentre(int widgetW, int widgetH) const;
    float stopCircleRadius(int widgetW, int widgetH) const;
    float pointerRadius(int widgetW, int widgetH) const;

    bool hitTest(const QPointF& pos, int widgetW, int widgetH) const override;
    bool handleMousePress(QMouseEvent* event, int widgetW, int widgetH) override;
    bool handleMouseRelease(QMouseEvent* event, int widgetW, int widgetH) override;
    bool handleMouseMove(QMouseEvent* event, int widgetW, int widgetH) override;

    // Multi-layer
    bool participatesIn(Layer layer) const override;
    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    void paintForLayer(QPainter& p, int widgetW, int widgetH, Layer layer) override;
    void paint(QPainter& p, int widgetW, int widgetH) override;

    QString serialize() const override;
    bool deserialize(const QString& data) override;

signals:
    // The dial changed outside a paint or a mouse event (a new heading
    // from the rotor); MeterWidget repaints it.
    void repaintRequested();

private:
    struct Faces {
        float w{0.0f};
        float h{0.0f};
        QPointF az;
        QPointF ele;
        float radiusTipAz{0.0f};
        float radiusExtraAz{0.0f};
        float radiusTipEle{0.0f};
        float radiusExtraEle{0.0f};
        float radiusStop{0.0f};
    };
    Faces faces(const QRect& faceRect) const;
    QRect faceRect(int widgetW, int widgetH) const;
    void paintControl(QPainter& p, const QRect& faceRect);
    void updateStopHover(const QPointF& pos, const Faces& f);
    void updateDrag(const QPointF& pos, const Faces& f);
    void sendStop();
    void sendTarget();
    void clearArrivedTargets();
    void cancelControl();
    void onRotorChanged();
    void feedTick();
    void paintCompassFace(QPainter& p, const QRect& compassRect);
    void paintHeading(QPainter& p, const QRect& compassRect);
    void paintElevationArc(QPainter& p, const QRect& eleRect);
    QRect squareRect(int widgetW, int widgetH) const;

    // From Thetis clsRotatorItem (MeterManager.cs:15095)
    RotatorMode m_mode{RotatorMode::Both};
    bool   m_showValue{true};
    bool   m_showCardinals{false};
    bool   m_showBeamWidth{false};
    float  m_beamWidth{30.0f};
    float  m_beamWidthAlpha{0.6f};
    bool   m_darkMode{false};
    float  m_padding{0.5f};

    // Colors (from Thetis clsRotatorItem MeterManager.cs:15100-15106)
    QColor m_bigBlobColour{0xff, 0x00, 0x00};     // Red
    QColor m_smallBlobColour{0xff, 0xff, 0xff};    // White
    QColor m_outerTextColour{0x80, 0x80, 0x80};    // Grey (128,128,128)
    QColor m_arrowColour{0xff, 0xff, 0xff};         // White
    QColor m_beamWidthColour{0x40, 0x40, 0x40};    // (64,64,64)
    QColor m_backgroundColour{0x20, 0x20, 0x20};   // (32,32,32)

    // Background image
    QString m_bgImagePath;
    QImage  m_bgImage;

    // Smoothed values (from Thetis clsRotatorItem Update() MeterManager.cs:15290-15312)
    float m_smoothedAz{0.0f};
    float m_elevation{0.0f};
    float m_smoothedEle{0.0f};

    // Rotor control plan Task 5: the rotor and the drag.
    enum class DragFace { None, Az, Ele };
    QPointer<RotorLink::RotorModel> m_rotor;
    RotorCommandSink* m_commands{nullptr};
    QMetaObject::Connection m_rotorState;
    QMetaObject::Connection m_rotorPosition;
    // The rotor's last heading, re-applied every kFeedIntervalMs until the
    // smoothed dial reaches it (Thetis re-reads every UpdateInterval).
    QTimer m_feed;
    float m_rotorAz{-1.0f};
    float m_rotorEle{-1.0f};
    bool m_pressed{false};
    bool m_overStop{false};
    QPointF m_lastPos;
    DragFace m_dragFace{DragFace::None};
    bool m_showingEleDrag{false};
    bool m_dragEle{false};
    float m_dragDegrees{kNoAngle};
    float m_targetAz{kNoAngle};
    float m_targetEle{kNoAngle};
};

} // namespace NereusSDR
