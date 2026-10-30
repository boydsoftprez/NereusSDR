# no-port-check: NereusSDR-original.
"""Maidenhead grid centers and great-circle distances (wire spec 4.5,
6.8), checked against values worked out here independently."""

import math

import pytest

from nereus_roundtable import grid


def _haversine(lat1, lon1, lat2, lon2):
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dp = p2 - p1
    dl = math.radians(lon2 - lon1)
    a = math.sin(dp / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2
    return 2 * 6371.0 * math.atan2(math.sqrt(a), math.sqrt(1 - a))


def test_square_centers():
    # EM73: field E (5th from A, 80 degrees east of 180 W) and M (13th, 120
    # degrees north of 90 S); square 7 and 3; the center is 1 degree of
    # longitude and half a degree of latitude in.
    assert grid.position("EM73") == (33.5, -85.0)
    assert grid.position("FN31") == (41.5, -73.0)
    assert grid.position("AA00") == (-89.5, -179.0)
    assert grid.position("RR99") == (89.5, 179.0)


def test_subsquare_centers_and_case():
    lat, lon = grid.position("EM73tu")
    # Subsquare t (19) by u (20): 5 minutes by 2.5 minutes, then half of each.
    assert lon == pytest.approx(-86.0 + 19 * 5 / 60 + 2.5 / 60)
    assert lat == pytest.approx(33.0 + 20 * 2.5 / 60 + 1.25 / 60)
    assert grid.position("em73TU") == grid.position("EM73tu")
    assert grid.position("fn31") == grid.position("FN31")


@pytest.mark.parametrize(
    "text",
    ["", "EM", "EM7", "EM73t", "EM73tu42", "EM73tu4", "SM73", "ES73", "EMA3", "EM73yx", "EM73xy", None, 42, " EM73"],
)
def test_invalid_grids(text):
    assert grid.position(text) is None
    assert grid.distance_km(text, "EM73") is None
    assert grid.distance_km("EM73", text) is None


def test_distance_matches_an_independent_haversine():
    want = _haversine(33.5, -85.0, 41.5, -73.0)
    got = grid.distance_km("EM73", "FN31")
    assert isinstance(got, int)
    assert abs(got - want) <= 1
    assert grid.distance_km("FN31", "EM73") == got


def test_distance_of_one_grid_is_zero_and_subsquares_count():
    assert grid.distance_km("EM73", "EM73") == 0
    lat, lon = grid.position("EM73tu")
    want = _haversine(lat, lon, 41.5, -73.0)
    assert abs(grid.distance_km("EM73tu", "FN31") - want) <= 1


def test_antipodes_fit_the_wire_range():
    # The wire's km is 0 to 20038.
    assert grid.distance_km("AA00", "RR99") <= 20038


@pytest.mark.parametrize("start,end,want", [("JJ00", "JJ10", 89.99), ("JJ00", "JJ01", 0.0), ("JJ10", "JJ00", 270.01), ("JJ01", "JJ00", 180.0)])
def test_initial_bearing_is_directed_true_degrees(start, end, want):
    assert grid.initial_bearing_deg(start, end) == pytest.approx(want, abs=0.02)


def test_initial_bearing_is_unknown_for_missing_or_coincident_positions():
    assert grid.initial_bearing_deg(None, "EM73") is None
    assert grid.initial_bearing_deg("EM73", "bad") is None
    assert grid.initial_bearing_deg("EM73", "EM73") is None


def test_initial_bearing_crosses_date_line_on_short_path():
    # Same latitude, neighboring squares across the date line.
    assert 89 < grid.initial_bearing_deg("RJ90", "AJ00") < 91
    assert 269 < grid.initial_bearing_deg("AJ00", "RJ90") < 271
