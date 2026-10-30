# no-port-check: NereusSDR-original.
"""Maidenhead grids and the distance between them (wire spec 4.5, 6.8).

Written from the Maidenhead locator definition: a field is two letters A to
R (20 degrees of longitude by 10 of latitude, from 180 W and 90 S), a square
two digits (2 degrees by 1), and a subsquare two letters A to X (5 minutes
of longitude by 2.5 of latitude). A grid stands for the center of its square
or subsquare. The distance is the haversine formula on a sphere of radius
6371 km, rounded to the nearest whole km.
"""

from __future__ import annotations

import math
import re
from typing import Optional, Tuple

EARTH_RADIUS_KM = 6371.0

_GRID = re.compile(r"\A([A-Ra-r])([A-Ra-r])([0-9])([0-9])(?:([A-Xa-x])([A-Xa-x]))?\Z")


def position(grid: object) -> Optional[Tuple[float, float]]:
    """The (latitude, longitude) in degrees of a 4 or 6 character grid's
    center, or None when it is not a grid."""
    if not isinstance(grid, str):
        return None
    m = _GRID.match(grid)
    if m is None:
        return None
    field_lon = ord(m.group(1).upper()) - ord("A")
    field_lat = ord(m.group(2).upper()) - ord("A")
    lon = -180.0 + field_lon * 20.0 + int(m.group(3)) * 2.0
    lat = -90.0 + field_lat * 10.0 + int(m.group(4)) * 1.0
    if m.group(5) is None:
        # The square's center: half of 2 degrees and half of 1.
        return lat + 0.5, lon + 1.0
    sub_lon = ord(m.group(5).upper()) - ord("A")
    sub_lat = ord(m.group(6).upper()) - ord("A")
    # A subsquare is 5 minutes (1/12 degree) by 2.5 minutes (1/24 degree).
    lon += sub_lon / 12.0 + 1.0 / 24.0
    lat += sub_lat / 24.0 + 1.0 / 48.0
    return lat, lon


def distance_km(a: object, b: object) -> Optional[int]:
    """The great-circle distance between two grids' centers in whole km, or
    None when either is not a grid."""
    pa = position(a)
    pb = position(b)
    if pa is None or pb is None:
        return None
    lat1, lon1 = math.radians(pa[0]), math.radians(pa[1])
    lat2, lon2 = math.radians(pb[0]), math.radians(pb[1])
    h = math.sin((lat2 - lat1) / 2) ** 2 + math.cos(lat1) * math.cos(lat2) * math.sin((lon2 - lon1) / 2) ** 2
    km = 2 * EARTH_RADIUS_KM * math.asin(min(1.0, math.sqrt(h)))
    return int(math.floor(km + 0.5))


def initial_bearing_deg(start: object, end: object) -> Optional[float]:
    """Initial great-circle bearing from start to end, clockwise from true
    north. Missing or coincident locator centers have no direction.

    Original spherical-geodesy implementation from the locator centers,
    not adapted from an upstream application.
    """
    a, b = position(start), position(end)
    if a is None or b is None or a == b:
        return None
    lat1, lon1 = map(math.radians, a)
    lat2, lon2 = map(math.radians, b)
    delta = lon2 - lon1
    east = math.sin(delta) * math.cos(lat2)
    north = math.cos(lat1) * math.sin(lat2) - math.sin(lat1) * math.cos(lat2) * math.cos(delta)
    # Antipodal centers do not define a unique initial course.
    if math.hypot(east, north) < 1e-12:
        return None
    return math.degrees(math.atan2(east, north)) % 360.0
