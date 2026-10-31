"""Keep public artifact names while Debian prereleases upgrade to the final."""

import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import textwrap

import pytest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/release-version.py"
STATION = ROOT / "packaging/station-image"


def convert(version):
    return subprocess.run(
        [sys.executable, str(SCRIPT), "debian-version", version],
        capture_output=True, text=True,
    )


@pytest.mark.parametrize("public,expected", [
    ("2026.10.2", "2026.10.2"),
    ("2026.10.2+git012345abcdef", "2026.10.2+git012345abcdef"),
    ("2026.10.2-rc1", "2026.10.2~rc1"),
    ("2026.10.2-rc10", "2026.10.2~rc10"),
    ("2026.10.2-alpha1", "2026.10.2~alpha1"),
    ("2026.10.2-beta2", "2026.10.2~beta2"),
    ("2026.10.2-issue83.fix", "2026.10.2~issue83.fix"),
])
def test_debian_version_keeps_stable_and_maps_accepted_suffixes(public, expected):
    result = convert(public)
    assert result.returncode == 0, result.stderr
    assert result.stdout == expected + "\n"
    assert result.stderr == ""


@pytest.mark.parametrize("public", ["2026.10.02", "2026.13.2", "2026.10.2-", "2026.10.2-rc_1"])
def test_debian_version_rejects_versions_the_release_checker_rejects(public):
    result = convert(public)
    assert result.returncode != 0
    assert result.stdout == ""


@pytest.mark.skipif(not shutil.which("dpkg"), reason="Debian ordering needs dpkg")
def test_debian_prerelease_order_allows_upgrade_to_final():
    versions = ["2026.10.2-alpha1", "2026.10.2-beta1", "2026.10.2-rc1",
                "2026.10.2-rc2", "2026.10.2-rc9", "2026.10.2-rc10", "2026.10.2"]
    mapped = []
    for version in versions:
        result = convert(version)
        assert result.returncode == 0, result.stderr
        mapped.append(result.stdout.strip())
    for older, newer in zip(mapped, mapped[1:]):
        subprocess.run(["dpkg", "--compare-versions", older, "lt", newer], check=True)


@pytest.mark.skipif(not shutil.which("dpkg-deb"), reason="Package metadata needs dpkg-deb")
@pytest.mark.parametrize("builder", ["release", "trixie"])
@pytest.mark.parametrize("public,expected", [
    ("2026.10.2", "2026.10.2"), ("2026.10.2-rc1", "2026.10.2~rc1"),
])
def test_builder_emits_debian_metadata_inside_publicly_named_package(tmp_path, builder, public, expected):
    # Execute the actual control-generation block, without running app builds,
    # dependency installs or package maintainer scripts. Only paths are redirected.
    if builder == "release":
        source = (ROOT / ".github/workflows/release.yml").read_text()
        source = textwrap.dedent(source.split("          sed -e", 1)[1].split("          install -m", 1)[0])
        block = "sed -e" + source
        workflow = (ROOT / ".github/workflows/release.yml").read_text()
        mapping = re.search(r'^\s*DEBIAN_VERSION=\$\(python3 scripts/release-version.py debian-version .*$', workflow, re.M)
        target = "debroot/DEBIAN/control"
    else:
        source = (STATION / "build-trixie-deb.sh").read_text()
        block = "sed -e" + source.split("sed -e", 1)[1].split("install -m", 1)[0]
        mapping = re.search(r'^DEBIAN_VERSION=\$\(python3 scripts/release-version.py debian-version .*$', source, re.M)
        target = "/tmp/nereus-debroot/DEBIAN/control"
    package_root = tmp_path / "package"
    control = package_root / "DEBIAN/control"
    control.parent.mkdir(parents=True)
    block = (mapping[0].strip() + "\n" if mapping else "") + block
    block = block.replace(target, str(control))
    env = dict(os.environ, VERSION=public, NEREUS_VERSION=public, ARCH="arm64",
               DEPS="libc6, libqt6core6t64", deps="libc6, libqt6core6t64")
    subprocess.run(["bash", "-eu", "-c", block], cwd=ROOT, env=env, check=True)
    package = tmp_path / f"nereusd_{public}_arm64{'_trixie' if builder == 'trixie' else ''}.deb"
    subprocess.run(["dpkg-deb", "--build", "--root-owner-group", str(package_root), str(package)], check=True)
    result = subprocess.run(["dpkg-deb", "--field", str(package), "Version"], capture_output=True, text=True, check=True)
    assert result.stdout.strip() == expected

    # Run the real verifier's version gate against the emitted package. Later
    # content/ELF checks concern the full application payload and are left intact.
    verifier = (STATION / "verify-package.sh").read_text().split("deps=$(", 1)[0]
    verifier = verifier.replace("${BASH_SOURCE[0]}", str(STATION / "verify-package.sh"))
    result = subprocess.run(["bash", "-eu", "-c", verifier, "verify-package.sh", str(package), public],
                            cwd=ROOT, capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    wrong = subprocess.run(["bash", "-eu", "-c", verifier, "verify-package.sh", str(package), "2026.10.3"],
                           cwd=ROOT, capture_output=True, text=True)
    assert wrong.returncode != 0

    if builder == "trixie":
        workflow = (ROOT / ".github/workflows/release.yml").read_text()
        (tmp_path / "scripts").symlink_to(ROOT / "scripts", target_is_directory=True)
        (tmp_path / "station-output").mkdir()
        package = package.rename(tmp_path / "station-output" / package.name)
        (tmp_path / "station-output/dfnr-provenance.json").write_text('{"fixture": true}')
        for name in ("station-builder-image.txt", "station-runtime-image.txt"):
            (tmp_path / name).write_text("fixture-image@sha256:" + "1" * 64)
        (tmp_path / "station-provenance").mkdir()
        step = workflow.split("      - name: Record Trixie package provenance\n", 1)[1]
        code = textwrap.dedent(step.split("          python3 - <<'PY'\n", 1)[1].split("          PY\n", 1)[0])
        env["SOURCE_SHA"] = "5" * 40
        subprocess.run([sys.executable, "-c", code], cwd=tmp_path, env=env, check=True)
        manifest = json.loads(next((tmp_path / "station-provenance").glob("*.json")).read_text())
        assert manifest["version"] == public
        assert manifest["debian_version"] == expected
        assert manifest["package"] == package.name
        assert manifest["source_sha"] == "5" * 40

        # Exercise the workflow's installed-version assertion using a query
        # fixture that reads the package metadata; no package is installed.
        query = tmp_path / "dpkg-query"
        query.write_text('#!/bin/sh\nexec dpkg-deb --field "$FIXTURE_PACKAGE" Version\n')
        query.chmod(0o755)
        env.update(PATH=str(tmp_path) + os.pathsep + os.environ["PATH"], FIXTURE_PACKAGE=str(package))
        mapping = re.search(r'^\s*NEREUS_DEBIAN_VERSION=\$\(python3 .*$', workflow, re.M)[0]
        assertion = re.search(r'^\s*test "\$\(dpkg-query .*Version.*$', workflow, re.M)[0]
        subprocess.run(["bash", "-eu", "-c", mapping + "\n" + assertion], cwd=tmp_path, env=env, check=True)
