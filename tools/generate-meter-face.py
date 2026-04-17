#!/usr/bin/env python3
"""
generate-meter-face.py — NereusSDR Multi-Meter face generator
Copyright (C) 2026 NereusSDR Contributors
SPDX-License-Identifier: GPL-2.0-or-later

Produces resources/meters/ananMM.png (1472×704, RGB PNG).
This is wholly original artwork authored by the NereusSDR project.
No ANAN/Apache Labs/OE3IDE assets were copied or derived.

Meter layout (all coordinates are in 0..1 unit space, matching
NeedleItem calibration points in ItemGroup.cpp):

  Top half  — large arc: Signal S-meter (RX) / Power / SWR (TX)
  Mid band  — arc band: Amps (TX)
  Lower band — arc band: Compression (TX) / ALC / Volts

The arcs are drawn at the same positions used by the calibration tables
in ItemGroup.cpp so the needle tips land on visible scale marks.

Palette (from StyleConstants.h / project guidelines):
  Background : #0f0f1a  (dark navy)
  Tick/labels: #c8d8e8  (light blue-grey)
  Accent     : #00b4d8  (cyan-blue)
  Red zone   : #e05050
  Panel lines: #1a2a3a
"""

import math
import sys
import os
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    sys.exit("ERROR: Pillow not installed. Run: pip3 install --user Pillow")

# ---------------------------------------------------------------------------
# Canvas constants
# ---------------------------------------------------------------------------
W, H = 1472, 704
BG_COLOR   = (0x0f, 0x0f, 0x1a)   # #0f0f1a dark navy
TICK_COLOR = (0xc8, 0xd8, 0xe8)   # #c8d8e8 light blue-grey
ACCENT     = (0x00, 0xb4, 0xd8)   # #00b4d8 cyan
RED_ZONE   = (0xe0, 0x50, 0x50)   # #e05050 red
PANEL      = (0x1a, 0x2a, 0x3a)   # #1a2a3a panel border
DIM_TEXT   = (0x70, 0x90, 0xa8)   # muted label


def px(nx: float, ny: float) -> tuple[int, int]:
    """Convert normalised 0..1 coordinates to pixel coordinates."""
    return (round(nx * W), round(ny * H))


def draw_arc_sector(draw: ImageDraw.ImageDraw,
                    cx: float, cy: float,
                    r_outer: float, r_inner: float,
                    start_deg: float, end_deg: float,
                    fill_color, outline_color=None, n_steps: int = 200):
    """
    Draw a filled arc sector (annular wedge) using polygon approximation.
    All coordinates in pixels.  Angles in degrees, measured from 3-o'clock,
    counter-clockwise (PIL convention is clockwise from 3-o'clock so we negate).
    """
    pts_outer = []
    pts_inner = []
    for i in range(n_steps + 1):
        t = start_deg + (end_deg - start_deg) * i / n_steps
        rad = math.radians(-t)           # PIL y-axis is down, so negate
        pts_outer.append((cx + r_outer * math.cos(rad),
                          cy + r_outer * math.sin(rad)))
        pts_inner.append((cx + r_inner * math.cos(rad),
                          cy + r_inner * math.sin(rad)))
    poly = pts_outer + list(reversed(pts_inner))
    draw.polygon(poly, fill=fill_color, outline=outline_color)


def draw_tick(draw: ImageDraw.ImageDraw,
              cx: float, cy: float,
              r_start: float, r_end: float,
              angle_deg: float,
              color, width: int = 2):
    """Draw a radial tick mark. angle_deg: from 3-o'clock, CCW in math space."""
    rad = math.radians(-angle_deg)
    x0 = cx + r_start * math.cos(rad)
    y0 = cy + r_start * math.sin(rad)
    x1 = cx + r_end   * math.cos(rad)
    y1 = cy + r_end   * math.sin(rad)
    draw.line([(x0, y0), (x1, y1)], fill=color, width=width)


def label_at_angle(draw: ImageDraw.ImageDraw,
                   cx: float, cy: float,
                   r_label: float,
                   angle_deg: float,
                   text: str, font, color):
    """Place centred text at a radius/angle from a pivot."""
    rad = math.radians(-angle_deg)
    x = cx + r_label * math.cos(rad)
    y = cy + r_label * math.sin(rad)
    bbox = draw.textbbox((0, 0), text, font=font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    draw.text((x - tw / 2, y - th / 2), text, font=font, fill=color)


# ---------------------------------------------------------------------------
# Load (or fallback) fonts
# ---------------------------------------------------------------------------
def get_font(size: int, bold: bool = False):
    search = [
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
    ]
    for p in search:
        if os.path.exists(p):
            try:
                from PIL import ImageFont as _IF
                return _IF.truetype(p, size)
            except Exception:
                continue
    from PIL import ImageFont as _IF
    return _IF.load_default()


# ---------------------------------------------------------------------------
# Main drawing routine
# ---------------------------------------------------------------------------
def generate(out_path: str):
    img  = Image.new("RGB", (W, H), BG_COLOR)
    draw = ImageDraw.Draw(img)

    # --- Fonts ---
    font_title  = get_font(36, bold=True)
    font_label  = get_font(26)
    font_small  = get_font(20)
    font_tiny   = get_font(16)

    # -----------------------------------------------------------------------
    # PANEL BACKGROUND — subtle gradient effect via rectangles
    # -----------------------------------------------------------------------
    # Outer border
    draw.rectangle([(4, 4), (W - 5, H - 5)], outline=PANEL, width=3)
    # Subtle inner fill gradient (top lighter)
    for row in range(H):
        frac = row / H
        shade = int(0x1a * (1 - frac * 0.4))
        r = min(0x0f + shade, 0x20)
        g = min(0x0f + shade, 0x20)
        b = min(0x1a + shade, 0x28)
        draw.line([(5, row), (W - 6, row)], fill=(r, g, b))

    # -----------------------------------------------------------------------
    # TITLE BAR
    # -----------------------------------------------------------------------
    title_h = 52
    draw.rectangle([(5, 5), (W - 6, title_h)], fill=(0x0a, 0x1a, 0x2a))
    draw.line([(5, title_h), (W - 6, title_h)], fill=ACCENT, width=2)

    title_text = "NereusSDR  Multi-Meter"
    bbox = draw.textbbox((0, 0), title_text, font=font_title)
    tw = bbox[2] - bbox[0]
    draw.text(((W - tw) // 2, 10), title_text, font=font_title, fill=ACCENT)

    # Mode labels
    draw.text((20, 14), "RX", font=font_label, fill=TICK_COLOR)
    draw.text((W - 60, 14), "TX", font=font_label, fill=TICK_COLOR)

    # -----------------------------------------------------------------------
    # LARGE UPPER ARC  — Signal (RX) / Power (TX) / SWR (TX)
    #
    # From ItemGroup.cpp signal calibration:
    #   leftmost tip  ~ (0.076, 0.31) in image-local coords
    #   rightmost tip ~ (0.926, 0.31)
    #   apex of arc   ~ (0.501, 0.142) at -73 dBm (mid S-meter)
    #
    # Derive pivot from these three sample points using circle fitting.
    # sharedOffset = (0.004, 0.736) means all needles pivot at x≈0,y≈0.736
    # sharedRadius = (1.0, 0.58)  so the radial reach is ~0.58 of height
    # -----------------------------------------------------------------------
    # Pivot in pixel space (shared for all needles per ItemGroup.cpp)
    piv_x = 0.004 * W      #  ~5.9 px
    piv_y = 0.736 * H      #  ~518 px

    # The arc extends roughly from angle 120° (left) to 60° (right) relative
    # to the pivot, measured CCW from 3-o'clock in math convention.
    # Empirically fitting to the calibration:
    #   Left  tip (0.076*W, 0.31*H):  angle ≈ 110°
    #   Right tip (0.926*W, 0.31*H):  angle ≈ 68°
    #   Apex (0.501*W, 0.142*H):      ≈ 88°
    # Radius ≈ distance from pivot to apex

    apex_px = (0.501 * W, 0.142 * H)
    r_apex = math.hypot(apex_px[0] - piv_x, apex_px[1] - piv_y)   # ≈ ~389 px

    # Arc sector: outer = r_apex + gutter, inner = r_apex - band_width
    arc_outer  = r_apex + 38
    arc_inner  = r_apex - 56
    arc_bg_in  = r_apex - 120   # background fill inner

    # Determine angular range from leftmost/rightmost calibration tips
    def cal_angle(nx, ny):
        """Angle in degrees (CCW from 3-o'clock) of a normalised tip position."""
        dx = nx * W - piv_x
        dy = ny * H - piv_y
        return math.degrees(math.atan2(-dy, dx))   # negate y for screen coords

    # S-meter calibration endpoints
    sig_angles = [
        (-127, 0.076, 0.31),
        (-121, 0.131, 0.272),
        (-115, 0.189, 0.254),
        (-109, 0.233, 0.211),
        (-103, 0.284, 0.207),
        ( -97, 0.326, 0.177),
        ( -91, 0.374, 0.177),
        ( -85, 0.414, 0.151),
        ( -79, 0.459, 0.168),
        ( -73, 0.501, 0.142),
        ( -63, 0.564, 0.172),
        ( -53, 0.630, 0.164),
        ( -43, 0.695, 0.203),
        ( -33, 0.769, 0.211),
        ( -23, 0.838, 0.272),
        ( -13, 0.926, 0.310),
    ]

    # Arc start/end from outermost calibration points
    ang_left  = cal_angle(0.076, 0.31)    # ≈ 110°
    ang_right = cal_angle(0.926, 0.31)    # ≈ 67°

    # ---- Draw the arc background (dark fill) ----
    draw_arc_sector(draw,
                    piv_x, piv_y,
                    arc_outer, arc_bg_in,
                    ang_right, ang_left,
                    fill_color=(0x10, 0x1e, 0x2c),
                    outline_color=None)

    # ---- Arc rim lines ----
    draw_arc_sector(draw,
                    piv_x, piv_y,
                    arc_outer, arc_outer - 4,
                    ang_right, ang_left,
                    fill_color=PANEL)
    draw_arc_sector(draw,
                    piv_x, piv_y,
                    arc_inner, arc_inner - 3,
                    ang_right, ang_left,
                    fill_color=ACCENT)

    # ---- Tick marks for Signal/Power scale ----
    # Map each calibration entry to an arc angle and draw a tick
    for (dbm, nx, ny) in sig_angles:
        a = cal_angle(nx, ny)
        # S-number: -127=-54dBm = S1, each 6dB = one S unit
        s_num = (dbm + 127) // 6 + 1   # crude mapping for labels
        is_major = (dbm in {-127, -121, -115, -109, -103, -97, -91, -85,
                             -79, -73, -63, -53, -43, -33, -23, -13})
        tick_len = 40 if is_major else 20
        color = RED_ZONE if dbm > -73 else TICK_COLOR
        draw_tick(draw, piv_x, piv_y, arc_inner - 4, arc_inner - 4 - tick_len, a,
                  color=color, width=3 if is_major else 2)

    # S-meter numeric labels
    s_labels = [
        (-127, "1"), (-109, "3"), (-91, "5"), (-79, "7"),
        (-73, "9"), (-63, "+10"), (-43, "+30"), (-13, "+60"),
    ]
    for (dbm, lbl) in s_labels:
        nx = next(x for (d, x, y) in sig_angles if d == dbm)
        ny = next(y for (d, x, y) in sig_angles if d == dbm)
        a  = cal_angle(nx, ny)
        r_lbl = arc_inner - 65
        label_at_angle(draw, piv_x, piv_y, r_lbl, a, lbl, font_small, TICK_COLOR)

    # "S" legend
    draw.text((int(0.12 * W), int(0.10 * H)), "S", font=font_label, fill=DIM_TEXT)

    # Power scale (TX) — reuse same arc, labelled inside when TX active
    pwr_labels = [
        (0.099, 0.352, "0"),
        (0.224, 0.280, "10"),
        (0.367, 0.228, "30"),
        (0.499, 0.212, "50"),
        (0.751, 0.272, "100"),
        (0.899, 0.352, "150"),
    ]
    for (nx, ny, lbl) in pwr_labels:
        a = cal_angle(nx, ny)
        r_lbl = arc_outer + 16
        label_at_angle(draw, piv_x, piv_y, r_lbl, a, lbl, font_tiny, DIM_TEXT)

    # "W" legend
    draw.text((int(0.88 * W), int(0.10 * H)), "W", font=font_label, fill=DIM_TEXT)

    # -----------------------------------------------------------------------
    # AMPS ARC — mid band
    # Calibration: y-range ≈ 0.492-0.576, x-range 0.199-0.799
    # -----------------------------------------------------------------------
    amp_angles = [
        ( 0, 0.199, 0.576),
        ( 2, 0.270, 0.540),
        ( 4, 0.333, 0.516),
        ( 6, 0.393, 0.504),
        ( 8, 0.448, 0.492),
        (10, 0.499, 0.492),
        (12, 0.554, 0.488),
        (14, 0.608, 0.500),
        (16, 0.667, 0.516),
        (18, 0.728, 0.540),
        (20, 0.799, 0.576),
    ]
    r_amp_mid = math.hypot(0.499 * W - piv_x, 0.492 * H - piv_y)
    amp_outer  = r_amp_mid + 20
    amp_inner  = r_amp_mid - 28
    amp_ang_l  = cal_angle(0.199, 0.576)
    amp_ang_r  = cal_angle(0.799, 0.576)

    draw_arc_sector(draw, piv_x, piv_y, amp_outer, amp_inner,
                    amp_ang_r, amp_ang_l,
                    fill_color=(0x0e, 0x1c, 0x28))
    draw_arc_sector(draw, piv_x, piv_y, amp_outer, amp_outer - 2,
                    amp_ang_r, amp_ang_l,
                    fill_color=PANEL)
    draw_arc_sector(draw, piv_x, piv_y, amp_inner, amp_inner - 2,
                    amp_ang_r, amp_ang_l,
                    fill_color=ACCENT)

    for (amp, nx, ny) in amp_angles:
        a = cal_angle(nx, ny)
        is_major = amp % 4 == 0
        draw_tick(draw, piv_x, piv_y, amp_inner - 2, amp_inner - 2 - (22 if is_major else 12), a,
                  color=TICK_COLOR, width=2 if is_major else 1)

    amp_labels = [(0, "0"), (5, ""), (10, "10"), (15, ""), (20, "20")]
    amp_cal_map = {a: (nx, ny) for (a, nx, ny) in amp_angles}
    for (amp, lbl) in amp_labels:
        if not lbl:
            continue
        # Interpolate if exact value not in table
        candidates = [(a, nx, ny) for (a, nx, ny) in amp_angles if abs(a - amp) < 1.5]
        if candidates:
            a_val, nx, ny = candidates[0]
            ang = cal_angle(nx, ny)
            label_at_angle(draw, piv_x, piv_y, amp_inner - 45, ang, lbl, font_tiny, TICK_COLOR)

    # "A" legend — placed left of amps arc
    draw.text((int(0.17 * W), int(0.50 * H)), "A", font=font_tiny, fill=DIM_TEXT)

    # -----------------------------------------------------------------------
    # SWR ARC — inset arc between Signal and Amps
    # Calibration y-range ≈ 0.360-0.476, x-range 0.152-0.847
    # -----------------------------------------------------------------------
    swr_angles = [
        ( 1.0, 0.152, 0.468),
        ( 1.5, 0.280, 0.404),
        ( 2.0, 0.393, 0.372),
        ( 2.5, 0.448, 0.360),
        ( 3.0, 0.499, 0.360),
        (10.0, 0.847, 0.476),
    ]
    r_swr_mid  = math.hypot(0.499 * W - piv_x, 0.360 * H - piv_y)
    swr_outer  = r_swr_mid + 22
    swr_inner  = r_swr_mid - 30
    swr_ang_l  = cal_angle(0.152, 0.468)
    swr_ang_r  = cal_angle(0.847, 0.476)

    draw_arc_sector(draw, piv_x, piv_y, swr_outer, swr_inner,
                    swr_ang_r, swr_ang_l,
                    fill_color=(0x0e, 0x1c, 0x28))
    draw_arc_sector(draw, piv_x, piv_y, swr_outer, swr_outer - 2,
                    swr_ang_r, swr_ang_l,
                    fill_color=PANEL)
    draw_arc_sector(draw, piv_x, piv_y, swr_inner, swr_inner - 2,
                    swr_ang_r, swr_ang_l,
                    fill_color=ACCENT)

    for (swr_val, nx, ny) in swr_angles:
        a = cal_angle(nx, ny)
        draw_tick(draw, piv_x, piv_y, swr_inner - 2, swr_inner - 2 - 20, a,
                  color=RED_ZONE if swr_val >= 3.0 else TICK_COLOR, width=2)

    swr_labels_txt = [(1.0, "1"), (2.0, "2"), (3.0, "3"), (10.0, "∞")]
    for (sv, lbl) in swr_labels_txt:
        candidates = [(s, nx, ny) for (s, nx, ny) in swr_angles if abs(s - sv) < 0.1]
        if candidates:
            _, nx, ny = candidates[0]
            ang = cal_angle(nx, ny)
            label_at_angle(draw, piv_x, piv_y, swr_inner - 48, ang, lbl, font_tiny, TICK_COLOR)

    draw.text((int(0.14 * W), int(0.38 * H)), "SWR", font=font_tiny, fill=DIM_TEXT)

    # -----------------------------------------------------------------------
    # COMPRESSION ARC — lower band
    # Calibration y-range ≈ 0.620-0.688, x-range 0.249-0.751
    # -----------------------------------------------------------------------
    comp_angles = [
        ( 0, 0.249, 0.680),
        ( 5, 0.342, 0.640),
        (10, 0.425, 0.624),
        (15, 0.499, 0.620),
        (20, 0.571, 0.628),
        (25, 0.656, 0.640),
        (30, 0.751, 0.688),
    ]
    r_comp_mid = math.hypot(0.499 * W - piv_x, 0.620 * H - piv_y)
    comp_outer = r_comp_mid + 18
    comp_inner = r_comp_mid - 24
    comp_ang_l = cal_angle(0.249, 0.680)
    comp_ang_r = cal_angle(0.751, 0.688)

    draw_arc_sector(draw, piv_x, piv_y, comp_outer, comp_inner,
                    comp_ang_r, comp_ang_l,
                    fill_color=(0x0e, 0x1c, 0x28))
    draw_arc_sector(draw, piv_x, piv_y, comp_outer, comp_outer - 2,
                    comp_ang_r, comp_ang_l, fill_color=PANEL)
    draw_arc_sector(draw, piv_x, piv_y, comp_inner, comp_inner - 2,
                    comp_ang_r, comp_ang_l, fill_color=ACCENT)

    for (cv, nx, ny) in comp_angles:
        a = cal_angle(nx, ny)
        draw_tick(draw, piv_x, piv_y, comp_inner - 2, comp_inner - 2 - 16, a,
                  color=TICK_COLOR, width=2)

    for (cv, nx, ny) in [(0, 0.249, 0.680), (15, 0.499, 0.620), (30, 0.751, 0.688)]:
        ang = cal_angle(nx, ny)
        label_at_angle(draw, piv_x, piv_y, comp_inner - 36, ang, str(cv), font_tiny, TICK_COLOR)

    draw.text((int(0.23 * W), int(0.64 * H)), "COMP dB", font=font_tiny, fill=DIM_TEXT)

    # -----------------------------------------------------------------------
    # ALC + VOLTS — bottom strip
    # ALC  calibration: y≈0.756-0.804, x 0.295-0.499
    # Volts calibration: y≈0.756-0.784, x 0.559-0.665
    # -----------------------------------------------------------------------
    # ALC arc
    alc_angles = [(-30, 0.295, 0.804), (0, 0.332, 0.784), (25, 0.499, 0.756)]
    r_alc_mid  = math.hypot(0.332 * W - piv_x, 0.784 * H - piv_y)
    alc_outer  = r_alc_mid + 16
    alc_inner  = r_alc_mid - 20
    alc_ang_l  = cal_angle(0.295, 0.804)
    alc_ang_r  = cal_angle(0.499, 0.756)

    draw_arc_sector(draw, piv_x, piv_y, alc_outer, alc_inner,
                    alc_ang_r, alc_ang_l,
                    fill_color=(0x0e, 0x1c, 0x28))
    draw_arc_sector(draw, piv_x, piv_y, alc_outer, alc_outer - 2,
                    alc_ang_r, alc_ang_l, fill_color=PANEL)
    draw_arc_sector(draw, piv_x, piv_y, alc_inner, alc_inner - 2,
                    alc_ang_r, alc_ang_l, fill_color=ACCENT)

    for (av, nx, ny) in alc_angles:
        a = cal_angle(nx, ny)
        draw_tick(draw, piv_x, piv_y, alc_inner - 2, alc_inner - 2 - 14, a,
                  color=TICK_COLOR, width=2)
        label_at_angle(draw, piv_x, piv_y, alc_inner - 30, a, str(av), font_tiny, TICK_COLOR)

    draw.text((int(0.26 * W), int(0.79 * H)), "ALC dB", font=font_tiny, fill=DIM_TEXT)

    # Volts arc
    volt_angles = [(10, 0.559, 0.756), (12.5, 0.605, 0.772), (15, 0.665, 0.784)]
    r_volt_mid  = math.hypot(0.605 * W - piv_x, 0.772 * H - piv_y)
    volt_outer  = r_volt_mid + 16
    volt_inner  = r_volt_mid - 20
    volt_ang_l  = cal_angle(0.559, 0.756)
    volt_ang_r  = cal_angle(0.665, 0.784)

    draw_arc_sector(draw, piv_x, piv_y, volt_outer, volt_inner,
                    volt_ang_r, volt_ang_l,
                    fill_color=(0x0e, 0x1c, 0x28))
    draw_arc_sector(draw, piv_x, piv_y, volt_outer, volt_outer - 2,
                    volt_ang_r, volt_ang_l, fill_color=PANEL)
    draw_arc_sector(draw, piv_x, piv_y, volt_inner, volt_inner - 2,
                    volt_ang_r, volt_ang_l, fill_color=ACCENT)

    for (vv, nx, ny) in volt_angles:
        a = cal_angle(nx, ny)
        draw_tick(draw, piv_x, piv_y, volt_inner - 2, volt_inner - 2 - 14, a,
                  color=TICK_COLOR, width=2)
        lbl = str(int(vv)) if vv == int(vv) else str(vv)
        label_at_angle(draw, piv_x, piv_y, volt_inner - 30, a, lbl, font_tiny, TICK_COLOR)

    draw.text((int(0.55 * W), int(0.79 * H)), "V", font=font_tiny, fill=DIM_TEXT)

    # -----------------------------------------------------------------------
    # PIVOT DOT — visual indicator of needle pivot position
    # -----------------------------------------------------------------------
    r_dot = 12
    draw.ellipse(
        [(piv_x - r_dot, piv_y - r_dot), (piv_x + r_dot, piv_y + r_dot)],
        fill=ACCENT, outline=TICK_COLOR
    )

    # -----------------------------------------------------------------------
    # BOTTOM INFO STRIP
    # -----------------------------------------------------------------------
    info_y = H - 26
    draw.line([(5, info_y - 6), (W - 6, info_y - 6)], fill=PANEL, width=1)
    draw.text((10, info_y - 4), "NereusSDR Multi-Meter  •  GPL-2.0-or-later",
              font=font_tiny, fill=DIM_TEXT)
    draw.text((W - 200, info_y - 4), "KG4VCF / NereusSDR",
              font=font_tiny, fill=DIM_TEXT)

    # -----------------------------------------------------------------------
    # SAVE
    # -----------------------------------------------------------------------
    img.save(out_path, "PNG")
    print(f"Saved {out_path}  ({W}×{H} RGB PNG)")


if __name__ == "__main__":
    repo_root = Path(__file__).resolve().parent.parent
    out = repo_root / "resources" / "meters" / "ananMM.png"
    generate(str(out))
