#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Modification history (NereusSDR):
#   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 20.
#                AI-assisted implementation via Anthropic Claude Code.
"""Makes each plugin's generated files from plugins/common/actions.json.

Run with no arguments to write them. With --check, write nothing and exit 1 if any
differs: Elgato's and Ulanzi's manifests, Ulanzi's en.json, StreamController's copy of the
action list, and Ulanzi's copy of link.js, which is Elgato's byte for byte.

Elgato: https://docs.elgato.com/streamdeck/sdk/references/manifest (fields, the characters
an action UUID may use, the $A1 layout). Ulanzi: libs/common-node/README.md (a main service
UUID has exactly 4 segments, an action more than 4) and AetherSDR
plugins/ulanzi-aethersdr/.../manifest.json [@1e0718ad] for the shape.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

PLUGINS = Path(__file__).resolve().parent.parent
ACTIONS = PLUGINS / "common" / "actions.json"
ELGATO = PLUGINS / "elgato-nereussdr" / "com.nereussdr.radio.sdPlugin"
ULANZI = PLUGINS / "ulanzi-nereussdr" / "com.nereussdr.radio.ulanziPlugin"
STREAMCONTROLLER = PLUGINS / "streamcontroller-nereussdr"

ELGATO_UUID = "com.nereussdr.radio"
ULANZI_UUID = "com.nereussdr.radio.controller"
URL = "https://github.com/boydsoftprez/NereusSDR"
AUTHOR = "J.J. Boyd (KG4VCF)"
DESCRIPTION = ("Untested. No real host has run this plugin yet. Runs NereusSDR's hot key actions from "
               "your keys and dials, through a private connection to the NereusSDR window on this computer.")


def controllers(action: dict) -> list[str]:
    # A dial turns only what tunes or sets a level. A hold on a dial, and anything that
    # keys, stays on keys.
    return ["Keypad", "Encoder"] if action["gesture"] == "turn" else ["Keypad"]


def elgato_action(action: dict) -> dict:
    entry = {
        "UUID": f"{ELGATO_UUID}.{action['id']}",
        "Name": action["name"],
        "Category": action["group"],
        "Icon": "imgs/plugin-icon",
        "Tooltip": action["name"],
        "Controllers": controllers(action),
        "PropertyInspectorPath": "pi/slice.html",
        "SupportedInMultiActions": False,
        "States": [{"Image": "imgs/plugin-icon"}],
    }
    if action["gesture"] == "turn":
        entry["Encoder"] = {"layout": "$A1", "TriggerDescription": {"Rotate": action["name"]}}
    return entry


def elgato_manifest(actions: list[dict]) -> dict:
    return {
        "$schema": "https://schemas.elgato.com/streamdeck/plugins/manifest.json",
        "UUID": ELGATO_UUID,
        "Version": "1.0.0.0",
        "Name": "NereusSDR",
        "Description": DESCRIPTION,
        "Author": AUTHOR,
        "URL": URL,
        "Icon": "imgs/plugin-icon",
        "CodePath": "plugin.js",
        "Category": "NereusSDR",
        "CategoryIcon": "imgs/plugin-icon",
        "SDKVersion": 2,
        "Software": {"MinimumVersion": "6.4"},
        "OS": [{"Platform": "mac", "MinimumVersion": "13"}, {"Platform": "windows", "MinimumVersion": "10"}],
        "Nodejs": {"Version": "20"},
        "Actions": [elgato_action(a) for a in actions],
    }


def ulanzi_manifest(actions: list[dict]) -> dict:
    return {
        "Version": "1.0.0",
        "Author": AUTHOR,
        "Name": "NereusSDR",
        "Description": DESCRIPTION,
        "Icon": "assets/icons/pluginIcon.png",
        "Category": "NereusSDR",
        "CategoryIcon": "assets/icons/category.png",
        "CodePath": "plugin/app.js",
        "Type": "JavaScript",
        "PrivateAPI": False,
        "SupportedInMultiActions": False,
        "UUID": ULANZI_UUID,
        "OS": [{"Platform": "windows", "MinimumVersion": "10"}, {"Platform": "mac", "MinimumVersion": "10.11"}],
        "Software": {"MinVersion": "2.1.4"},
        "Actions": [{
            "Name": a["name"],
            "Icon": "assets/icons/action.png",
            "PropertyInspectorPath": "property-inspector/slice.html",
            "Controllers": controllers(a),
            "Tooltip": a["name"],
            "UUID": f"{ULANZI_UUID}.{a['id']}",
            "SupportedInMultiActions": False,
            "States": [{"Image": "assets/icons/action.png"}],
        } for a in actions],
    }


def ulanzi_strings(actions: list[dict]) -> dict:
    return {
        "Name": "NereusSDR",
        "Description": DESCRIPTION,
        "Actions": [{"Name": a["name"], "Tooltip": a["name"]} for a in actions],
        "Localization": {"Slice": "Slice", "The slice the window is on": "The slice the window is on"},
    }


def as_text(content: dict) -> str:
    return json.dumps(content, indent=2, ensure_ascii=False) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="write nothing; exit 1 if anything differs")
    args = parser.parse_args()

    actions_text = ACTIONS.read_text(encoding="utf-8")
    actions = json.loads(actions_text)
    bad_ids = [a["id"] for a in actions if not re.fullmatch(r"[a-z0-9.-]+", a["id"])]
    if bad_ids:
        print(f"Action ids an Elgato UUID can't hold: {bad_ids}", file=sys.stderr)
        return 1

    wanted = {
        ELGATO / "manifest.json": as_text(elgato_manifest(actions)),
        ULANZI / "manifest.json": as_text(ulanzi_manifest(actions)),
        ULANZI / "en.json": as_text(ulanzi_strings(actions)),
        STREAMCONTROLLER / "actions" / "actions.json": actions_text,
        ULANZI / "plugin" / "link.js": (ELGATO / "link.js").read_text(encoding="utf-8"),
    }
    stale = []
    for path, text in wanted.items():
        if args.check:
            if not path.is_file() or path.read_text(encoding="utf-8") != text:
                stale.append(path)
        else:
            path.write_text(text, encoding="utf-8")
    for path in stale:
        print(f"Out of date: {path}. Run plugins/tools/gen-manifests.py", file=sys.stderr)
    return 1 if stale else 0


if __name__ == "__main__":
    sys.exit(main())
