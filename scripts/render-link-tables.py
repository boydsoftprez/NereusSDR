#!/usr/bin/env python3
# no-port-check: NereusSDR-original.
"""Render the station link document's tables from surface.json.

docs/architecture/2026-09-23-station-link-v1.md holds prose written by hand
and tables generated from tests/data/link/v1/surface.json, the link's
surface as tst_link_surface_manifest captures it from the code. Each table
block sits between two markers:

    <!-- surface:<section> -->
    ...generated...
    <!-- /surface -->

where <section> is one of surface.json's nine top-level keys, or one of the
derived blocks below that render a second table from one of those keys.
Every key and every derived block must have exactly one block, so a new
section in surface.json cannot go unrendered.

Derived blocks:
    capabilityVersions   section 6.3: each per-feature capability version
                         and the value the capture fixture advertises, or
                         its explicit absence (from "capabilities")

Usage:
    python3 scripts/render-link-tables.py            rewrite the document
    python3 scripts/render-link-tables.py --check    exit 1 if it is stale
    --surface PATH   read another surface.json (default: the committed one)
    --doc PATH       render another document (default: the link document)

The renderer knows every field surface.json carries and refuses one it does
not know (exit 2), so an edit to surface.json always changes the rendering
or stops the check. Python standard library only.
"""

# Modification history (NereusSDR):
#   2026-10-04  J.J. Boyd / KG4VCF  Render declaration-only watch capability
#                                  without claiming a sampled value.
#                                  AI-assisted via OpenAI Codex.

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SURFACE = ROOT / "tests" / "data" / "link" / "v1" / "surface.json"
DEFAULT_DOC = ROOT / "docs" / "architecture" / "2026-09-23-station-link-v1.md"

SECTIONS = (
    "messageKinds",
    "capabilities",
    "mirrorClasses",
    "objectKeys",
    "commands",
    "settingsScope",
    "telemetry",
    "mediaControl",
    "limits",
)

BLOCK = re.compile(
    r"(<!-- surface:(?P<name>[A-Za-z]+) -->\n)(?P<body>.*?)(<!-- /surface -->)",
    re.DOTALL,
)


class SurfaceError(Exception):
    """surface.json holds something this renderer does not know."""


def expect_keys(obj, allowed, where, required=None):
    if not isinstance(obj, dict):
        raise SurfaceError(f"{where}: expected an object")
    unknown = sorted(set(obj) - set(allowed))
    if unknown:
        raise SurfaceError(f"{where}: unknown field(s) {', '.join(unknown)}")
    missing = sorted(set(required if required is not None else allowed) - set(obj))
    if missing:
        raise SurfaceError(f"{where}: missing field(s) {', '.join(missing)}")


def cell(text):
    return str(text).replace("|", "\\|").replace("\n", " ")


def code(text):
    return f"`{text}`"


def code_list(items):
    return ", ".join(code(i) for i in items) if items else "none"


def table(header, rows):
    lines = ["| " + " | ".join(header) + " |",
             "| " + " | ".join("---" for _ in header) + " |"]
    for row in rows:
        lines.append("| " + " | ".join(cell(c) for c in row) + " |")
    return "\n".join(lines)


def render_message_kinds(data):
    rows = []
    for kind in sorted(data):
        entry = data[kind]
        expect_keys(entry, ("required", "optional", "types"), f"messageKinds.{kind}")
        types = entry["types"]
        expect_keys(types, tuple(entry["required"]) + tuple(entry["optional"]),
                    f"messageKinds.{kind}.types")

        def typed(keys):
            return ", ".join(f"{code(k)} ({types[k]})" for k in keys) if keys else "none"

        rows.append((code(kind), typed(entry["required"]), typed(entry["optional"])))
    return table(("Kind (`type`)", "Required keys (JSON type)", "Optional keys (JSON type)"),
                 rows)


def validate_capability(entry, where):
    # Only this route-specific declaration may lack a sampled live value.
    # All other capability fields and unknown-field rejection stay strict.
    expect_keys(entry, ("name", "kind", "value"), where, required=("name", "kind"))
    if entry["name"] == "txWatchPathVersion":
        if entry["kind"] != "i64":
            raise SurfaceError(f"{where}: txWatchPathVersion requires wire kind i64")
    else:
        expect_keys(entry, ("name", "kind", "value"), where)


def render_capabilities(data):
    rows = []
    for index, entry in enumerate(data, start=1):
        validate_capability(entry, f"capabilities[{index - 1}]")
        rows.append((index, code(entry["name"]), code(entry["kind"])))
    return table(("Order", "Name", "Wire kind"), rows)


def is_feature_version(entry):
    # A per-feature capability version (section 6.1's rule), and txPermitted,
    # the one flag a client gates on. settingsSchemaVersion is the settings
    # store's own version and firmwareVersion the radio's text, not features.
    name = entry["name"]
    return (entry["kind"] == "i64" and name.endswith("Version")
            and name != "settingsSchemaVersion") or name == "txPermitted"


def render_capability_versions(data):
    rows = []
    for index, entry in enumerate(data):
        validate_capability(entry, f"capabilities[{index}]")
        if not is_feature_version(entry):
            continue
        if "value" not in entry:
            rows.append((code(entry["name"]), "not advertised by this fixture"))
            continue
        value = entry["value"]
        if isinstance(value, bool):
            shown = "true" if value else "false"
        else:
            shown = value
        rows.append((code(entry["name"]), shown))
    return table(("Capability", "Value advertised by capture fixture"), rows)


def render_mirror_classes(data):
    parts = []
    for name in sorted(data):
        entry = data[name]
        expect_keys(entry, ("properties",), f"mirrorClasses.{name}")
        rows = []
        for prop in entry["properties"]:
            where = f"mirrorClasses.{name}.{prop.get('name', '?')}"
            expect_keys(prop, ("ordinal", "name", "kind", "direction", "enumValues"), where,
                        required=("ordinal", "name", "kind", "direction"))
            values = prop.get("enumValues")
            rows.append((prop["ordinal"], code(prop["name"]), code(prop["kind"]),
                         prop["direction"],
                         ", ".join(str(v) for v in values) if values is not None else ""))
        parts.append(f"**{name}** ({len(rows)} properties)\n\n"
                     + table(("Ordinal", "Property", "Wire kind", "Direction", "Enum values"),
                             rows))
    return "\n\n".join(parts)


def render_object_keys(data):
    rows = []
    for index, entry in enumerate(data):
        expect_keys(entry, ("key", "class"), f"objectKeys[{index}]")
        rows.append((code(entry["key"]), code(entry["class"])))
    return table(("Object key", "Class"), rows)


def render_commands(data):
    rows = []
    for entry in data:
        where = f"commands[{entry.get('verb', '?')}]"
        expect_keys(entry, ("verb", "arguments", "capability", "capabilityVersion", "minMinor"),
                    where)
        args = []
        for arg in entry["arguments"]:
            expect_keys(arg, ("name", "kind", "optional"), f"{where}.arguments")
            args.append(f"`{arg['name']}` {arg['kind']}"
                        + (" (optional)" if arg["optional"] else ""))
        capability = entry["capability"]
        rows.append((code(entry["verb"]), ", ".join(args) if args else "none",
                     code(capability) if capability else "none",
                     entry["capabilityVersion"], entry["minMinor"]))
    return table(("Verb", "Arguments", "Capability", "Capability version", "Minimum minor"),
                 rows)


def render_settings_scope(data):
    expect_keys(data, ("exceptions", "stationPrefixes", "operatorLocalPrefixes",
                       "stationKeys", "operatorLocalKeys"), "settingsScope")
    rows = []
    for index, entry in enumerate(data["exceptions"]):
        expect_keys(entry, ("key", "scope"), f"settingsScope.exceptions[{index}]")
        rows.append(("1. exact key (exception)", code(entry["key"]), entry["scope"]))
    for text in data["stationPrefixes"]:
        rows.append(("2. prefix", code(text), "station"))
    for text in data["operatorLocalPrefixes"]:
        rows.append(("2. prefix", code(text), "operatorLocal"))
    for text in data["stationKeys"]:
        rows.append(("3. whole key", code(text), "station"))
    for text in data["operatorLocalKeys"]:
        rows.append(("3. whole key", code(text), "operatorLocal"))
    return table(("Tier", "Key or prefix", "Scope"), rows)


def render_telemetry(data):
    expect_keys(data, ("kind", "versions"), "telemetry")
    rows = []
    seen = set()
    for entry in data["versions"]:
        expect_keys(entry, ("version", "minMinor", "fields"), "telemetry.versions")
        added = [f for f in entry["fields"] if f not in seen]
        seen.update(entry["fields"])
        rows.append((entry["version"], entry["minMinor"], len(entry["fields"]),
                     code_list(added)))
    return (f"Message kind {code(data['kind'])}.\n\n"
            + table(("`stationTelemetryVersion`", "Minimum minor", "Field paths",
                     "Field paths added"), rows))


def render_media_direction(ops, where):
    rows = []
    for op in sorted(ops):
        entry = ops[op]
        expect_keys(entry, ("capability", "fields", "conditional", "nested"), f"{where}.{op}",
                    required=("capability", "fields"))
        conditional = entry.get("conditional", {})
        cond = "; ".join(f"`{field}` with {', '.join(conditional[field])}"
                         for field in sorted(conditional))
        nested = entry.get("nested", {})
        nest = "; ".join(f"`{field}`: " + " or ".join("{" + ", ".join(shape) + "}"
                                                       for shape in nested[field])
                         for field in sorted(nested))
        rows.append((code(op), code(entry["capability"]), code_list(entry["fields"]),
                     cond or "none", nest or "none"))
    return table(("Operation (`op`)", "Capability", "Fields always present",
                  "Fields present with", "Object-valued fields"), rows)


def render_media_control(data):
    expect_keys(data, ("kind", "guiToCore", "coreToGui"), "mediaControl")
    return (f"Message kind {code(data['kind'])}; the operation's keys sit in `payload`.\n\n"
            "Client to station:\n\n"
            + render_media_direction(data["guiToCore"], "mediaControl.guiToCore")
            + "\n\nStation to client:\n\n"
            + render_media_direction(data["coreToGui"], "mediaControl.coreToGui"))


def render_limits(data):
    rows = []
    for name in sorted(data):
        entry = data[name]
        expect_keys(entry, ("value", "unit", "source"), f"limits.{name}")
        value = entry["value"]
        if isinstance(value, dict):
            expect_keys(value, ("min", "max"), f"limits.{name}.value")
            shown = f"{value['min']} to {value['max']}"
        elif isinstance(value, list):
            shown = ", ".join(str(v) for v in value)
        else:
            shown = str(value)
        rows.append((code(name), shown, entry["unit"], entry["source"]))
    return table(("Limit", "Value", "Unit", "Source in the code"), rows)


# Blocks that render a second table from one of the sections.
DERIVED = {
    "capabilityVersions": ("capabilities", render_capability_versions),
}

RENDERERS = {
    "messageKinds": render_message_kinds,
    "capabilities": render_capabilities,
    "mirrorClasses": render_mirror_classes,
    "objectKeys": render_object_keys,
    "commands": render_commands,
    "settingsScope": render_settings_scope,
    "telemetry": render_telemetry,
    "mediaControl": render_media_control,
    "limits": render_limits,
}


def render(document, surface):
    expect_keys(surface, SECTIONS, "surface.json")
    found = [m.group("name") for m in BLOCK.finditer(document)]
    blocks = SECTIONS + tuple(DERIVED)
    unknown = sorted(set(found) - set(blocks))
    if unknown:
        raise SurfaceError(f"document has block(s) for unknown section(s): {', '.join(unknown)}")
    counts = {name: found.count(name) for name in blocks}
    wrong = [name for name, n in counts.items() if n != 1]
    if wrong:
        raise SurfaceError("each section needs exactly one block; wrong count for "
                           + ", ".join(f"{n} ({counts[n]})" for n in wrong))

    def replace(match):
        name = match.group("name")
        if name in DERIVED:
            section, renderer = DERIVED[name]
            body = renderer(surface[section])
        else:
            body = RENDERERS[name](surface[name])
        return (match.group(1)
                + "<!-- Generated by scripts/render-link-tables.py from "
                  "tests/data/link/v1/surface.json. Do not edit by hand. -->\n\n"
                + body + "\n\n" + match.group(4))

    return BLOCK.sub(replace, document)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--check", action="store_true",
                        help="exit 1 if the document differs from its rendering")
    parser.add_argument("--surface", type=Path, default=DEFAULT_SURFACE)
    parser.add_argument("--doc", type=Path, default=DEFAULT_DOC)
    args = parser.parse_args()

    try:
        surface = json.loads(args.surface.read_text(encoding="utf-8"))
        document = args.doc.read_text(encoding="utf-8")
        rendered = render(document, surface)
    except (OSError, json.JSONDecodeError, SurfaceError) as error:
        print(f"render-link-tables: {error}", file=sys.stderr)
        return 2

    if args.check:
        if rendered != document:
            print(f"render-link-tables: {args.doc} is out of date with {args.surface}; "
                  "run python3 scripts/render-link-tables.py", file=sys.stderr)
            return 1
        print(f"render-link-tables: {args.doc.name} matches {args.surface.name}")
        return 0

    if rendered != document:
        args.doc.write_text(rendered, encoding="utf-8")
        print(f"render-link-tables: rewrote {args.doc}")
    else:
        print(f"render-link-tables: {args.doc.name} already up to date")
    return 0


if __name__ == "__main__":
    sys.exit(main())
