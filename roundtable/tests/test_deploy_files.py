# no-port-check: NereusSDR-original.
"""The deploy files: the systemd units and timers, the Caddy site file and
drop-in, the website Caddyfile's import line, the dry runs of
setup-server.sh, deploy.sh and pull-backup.sh, and the fixtures published
on the website (design 6.2, C10, C12; R-RT-25)."""

from __future__ import annotations

import filecmp
import os
import re
import shutil
import sqlite3
import stat
import subprocess
from pathlib import Path
from typing import Dict, List, Tuple

import pytest

ROOT = Path(__file__).resolve().parents[2]
DEPLOY = ROOT / "roundtable" / "deploy"
WEBSITE_CADDYFILE = ROOT / "website" / "deploy" / "Caddyfile"
RV_UNIT = ROOT / "rendezvous" / "deploy" / "nereus-rendezvous.service"
RV_CADDY_DROPIN = ROOT / "rendezvous" / "deploy" / "caddy-override.conf"
IMPORT_LINE = "import /etc/caddy/sites/*.caddy"
# The website's own Content-Security-Policy, which Roundtable leaves alone.
WEBSITE_CSP = (
    '\t\tContent-Security-Policy "default-src \'self\'; script-src \'self\'; style-src \'self\' '
    "https://fonts.googleapis.com; font-src 'self' https://fonts.gstatic.com; img-src 'self' data:; "
    "connect-src 'self' https://api.github.com; frame-ancestors 'none'; base-uri 'self'; "
    "form-action 'none'; object-src 'none'\""
)


def unit_entries(path: Path) -> List[Tuple[str, str, str]]:
    """Every (section, key, value) of a systemd unit, in order; a key may
    repeat."""
    section = ""
    out = []
    for raw in path.read_text("utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            section = line[1:-1]
            continue
        key, _, value = line.partition("=")
        out.append((section, key, value))
    return out


def unit_values(path: Path, section: str = "Service") -> Dict[str, List[str]]:
    values: Dict[str, List[str]] = {}
    for sec, key, value in unit_entries(path):
        if sec == section:
            values.setdefault(key, []).append(value)
    return values


def rv_sandbox() -> Dict[str, str]:
    """The rendezvous unit's sandbox options: its "Sandboxing" part."""
    text = RV_UNIT.read_text("utf-8")
    part = text[text.index("# --- Sandboxing") : text.index("[Install]")]
    out = {}
    for raw in part.splitlines():
        line = raw.strip()
        if line and not line.startswith("#"):
            key, _, value = line.partition("=")
            out[key] = value
    return out


def block(text: str, opener: str) -> str:
    """The text inside the braces that follow `opener` in a Caddyfile."""
    start = text.index(opener)
    i = text.index("{", start)
    depth = 0
    for j in range(i, len(text)):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                return text[i + 1 : j]
    raise AssertionError(f"unclosed block after {opener!r}")


def uncommented(text: str) -> str:
    return "\n".join(line for line in text.splitlines() if not line.lstrip().startswith("#"))


def tree(folder: Path) -> Dict[str, bytes]:
    return {p.relative_to(folder).as_posix(): (p.read_bytes() if p.is_file() else b"<dir>") for p in sorted(folder.rglob("*"))}


# ---------------------------------------------------------------- units


def test_server_unit_has_every_required_key() -> None:
    values = unit_values(DEPLOY / "nereus-roundtable.service")
    assert values["ExecStart"] == ["/usr/bin/python3 -m nereus_roundtable --config /etc/nereus-roundtable/roundtable.conf"]
    assert "PYTHONPATH=/opt/nereus-roundtable" in values["Environment"]
    assert values["DynamicUser"] == ["yes"]
    assert values["StateDirectory"] == ["nereus-roundtable"]
    assert values["MemoryMax"] == ["256M"]
    assert values["MemorySwapMax"] == ["0"]
    assert values["OOMScoreAdjust"] == ["500"]
    assert values["Restart"] == ["on-failure"]
    # Loopback only, and no outbound network.
    assert values["IPAddressDeny"] == ["any"]
    assert values["IPAddressAllow"] == ["localhost"]
    install = unit_values(DEPLOY / "nereus-roundtable.service", "Install")
    assert install["WantedBy"] == ["multi-user.target"]


def test_server_unit_carries_the_rendezvous_sandbox() -> None:
    values = unit_values(DEPLOY / "nereus-roundtable.service")
    sandbox = rv_sandbox()
    assert len(sandbox) >= 20
    for key, value in sandbox.items():
        assert values.get(key) == [value], key


def test_job_units_run_one_job_as_the_server_account() -> None:
    server = unit_values(DEPLOY / "nereus-roundtable.service")
    for job, flag in (("daily", "--daily"), ("snapshot", "--snapshot")):
        values = unit_values(DEPLOY / f"nereus-roundtable-{job}.service")
        assert values["Type"] == ["oneshot"]
        assert values["ExecStart"] == [
            f"/usr/bin/python3 -m nereus_roundtable --config /etc/nereus-roundtable/roundtable.conf {flag}"
        ]
        assert "PYTHONPATH=/opt/nereus-roundtable" in values["Environment"]
        for key in ("DynamicUser", "User", "Group", "StateDirectory", "MemoryMax"):
            assert values[key] == server[key], (job, key)
        assert values["IPAddressDeny"] == ["any"]
        assert values["PrivateNetwork"] == ["yes"]


def test_timers_run_at_their_hours() -> None:
    daily = unit_values(DEPLOY / "nereus-roundtable-daily.timer", "Timer")
    snapshot = unit_values(DEPLOY / "nereus-roundtable-snapshot.timer", "Timer")
    assert daily["OnCalendar"] == ["*-*-* 04:00:00 UTC"]
    assert snapshot["OnCalendar"] == ["*-*-* 03:30:00 UTC"]
    for name in ("daily", "snapshot"):
        timer = DEPLOY / f"nereus-roundtable-{name}.timer"
        assert unit_values(timer, "Timer")["Persistent"] == ["true"]
        assert unit_values(timer, "Install")["WantedBy"] == ["timers.target"]
        # A timer starts the service of its own name.
        assert "Unit" not in unit_values(timer, "Timer")


# ---------------------------------------------------------------- Caddy


def test_caddy_site_routes_the_websocket_paths_and_the_rest() -> None:
    text = uncommented((DEPLOY / "roundtable.caddy").read_text("utf-8"))
    site = block(text, "roundtable.nereussdr.com {")
    assert re.search(r"^\s*@websocket path /v1/station\* /v1/live\*$", site, re.M)
    ws = block(site, "handle @websocket {")
    ws_proxy = block(ws, "reverse_proxy 127.0.0.1:8720 [::1]:8720 {")
    assert "stream_close_delay 5m" in ws_proxy
    assert "header_up X-Forwarded-For {remote_host}" in ws_proxy
    rest = block(site, "\thandle {")
    rest_proxy = block(rest, "reverse_proxy 127.0.0.1:8721 [::1]:8721 {")
    assert "header_up X-Forwarded-For {remote_host}" in rest_proxy
    assert "8720" not in rest
    # No access log, and no global options of its own.
    assert not re.search(r"^\s*log\b", text, re.M)
    assert not text.lstrip().startswith("{")


def test_caddy_dropin_matches_the_rendezvous_one() -> None:
    values = unit_values(DEPLOY / "caddy-override.conf")
    assert values["Restart"] == ["on-failure"]
    assert values["OOMScoreAdjust"] == ["-100"]
    assert values["Environment"] == ["GOMEMLIMIT=@GOMEMLIMIT@"]
    assert values == unit_values(RV_CADDY_DROPIN)


def test_website_caddyfile_imports_the_sites_after_its_global_block() -> None:
    text = WEBSITE_CADDYFILE.read_text("utf-8")
    lines = text.splitlines()
    assert lines.count(IMPORT_LINE) == 1
    at = lines.index(IMPORT_LINE)
    global_end = next(i for i, line in enumerate(lines) if line == "}")
    first_site = next(i for i, line in enumerate(lines) if line.startswith("(site_headers)"))
    assert global_end < at < first_site
    # The website's own policy is unchanged.
    assert lines.count(WEBSITE_CSP) == 1


def test_website_setup_makes_the_sites_folder_before_validating() -> None:
    text = (ROOT / "website" / "deploy" / "setup-server.sh").read_text("utf-8")
    made = text.index('install -d -m 0755 -o root -g root "$CADDY_SITES_DIR"')
    assert 'readonly CADDY_SITES_DIR="/etc/caddy/sites"' in text
    assert made < text.index("caddy validate --config")


def test_caddy_validates_the_website_with_the_site_file(tmp_path: Path) -> None:
    caddy = shutil.which("caddy")
    if caddy is None:
        pytest.skip(
            "caddy is not installed here, so caddy validate cannot run. CI does not install it: Ubuntu 24.04 apt"
            " has Caddy 2.6, and the site file uses stream_close_delay (Caddy 2.7), so this check is local only"
        )
    sites = tmp_path / "sites"
    sites.mkdir()
    shutil.copy(DEPLOY / "roundtable.caddy", sites / "roundtable.caddy")
    text = WEBSITE_CADDYFILE.read_text("utf-8").replace(IMPORT_LINE, f"import {sites}/*.caddy")
    caddyfile = tmp_path / "Caddyfile"
    caddyfile.write_text(text, "utf-8")
    run = subprocess.run(
        [caddy, "validate", "--config", str(caddyfile), "--adapter", "caddyfile"],
        capture_output=True,
        text=True,
        timeout=120,
    )
    assert run.returncode == 0, run.stdout + run.stderr


# ---------------------------------------------------------------- scripts


def run_setup(root: Path, *args: str) -> subprocess.CompletedProcess:
    env = dict(os.environ, RT_ROOT=str(root))
    env.pop("RT_HOST", None)
    return subprocess.run(
        ["bash", str(DEPLOY / "setup-server.sh"), *args],
        capture_output=True,
        text=True,
        env=env,
        timeout=120,
    )


def test_setup_dry_run_changes_nothing_and_prints_its_steps(tmp_path: Path) -> None:
    run = run_setup(tmp_path, "--dry-run")
    assert run.returncode == 0, run.stdout + run.stderr
    steps = re.findall(r"^==> (\d)/8 ", run.stdout, re.M)
    assert steps == [str(i) for i in range(1, 9)]
    assert "Dry run only: nothing was changed." in run.stdout
    for name in ("nereus-roundtable.service", "nereus-roundtable-daily.timer", "roundtable.caddy", "roundtable.conf"):
        assert re.search(re.escape(name) + r" \(dry run\): new$", run.stdout, re.M), name
    assert "GOMEMLIMIT" in run.stdout
    assert list(tmp_path.iterdir()) == []


def test_setup_dry_run_over_an_installed_server_finds_it_the_same(tmp_path: Path) -> None:
    """Files as a run would leave them: a second run has nothing to do,
    and the dry run still writes nothing."""
    units = tmp_path / "etc" / "systemd" / "system"
    units.mkdir(parents=True)
    for name in (
        "nereus-roundtable.service",
        "nereus-roundtable-daily.service",
        "nereus-roundtable-daily.timer",
        "nereus-roundtable-snapshot.service",
        "nereus-roundtable-snapshot.timer",
    ):
        shutil.copy(DEPLOY / name, units / name)
    (units / "caddy.service.d").mkdir()
    (units / "caddy.service.d" / "nereus.conf").write_text(
        (DEPLOY / "caddy-override.conf").read_text("utf-8").replace("@GOMEMLIMIT@", "352MiB"), "utf-8"
    )
    (tmp_path / "etc" / "caddy" / "sites").mkdir(parents=True)
    shutil.copy(WEBSITE_CADDYFILE, tmp_path / "etc" / "caddy" / "Caddyfile")
    shutil.copy(DEPLOY / "roundtable.caddy", tmp_path / "etc" / "caddy" / "sites" / "roundtable.caddy")
    (tmp_path / "etc" / "nereus-roundtable").mkdir()
    conf = tmp_path / "etc" / "nereus-roundtable" / "roundtable.conf"
    conf.write_text("[roundtable]\npublic_host = roundtable.nereussdr.com\nhelpers = abc\n", "utf-8")
    (tmp_path / "opt" / "nereus-roundtable").mkdir(parents=True)
    before = tree(tmp_path)
    env_memory = dict(os.environ, RT_MEMORY_MB="961")
    run = subprocess.run(
        ["bash", str(DEPLOY / "setup-server.sh"), "--dry-run"],
        capture_output=True,
        text=True,
        env=dict(env_memory, RT_ROOT=str(tmp_path)),
        timeout=120,
    )
    assert run.returncode == 0, run.stdout + run.stderr
    assert not re.search(r"\(dry run\): (new|changed)$", run.stdout, re.M), run.stdout
    assert "present; left as it is" in run.stdout
    assert "act on: nothing" in run.stdout
    assert tree(tmp_path) == before


def test_setup_refuses_a_real_run_off_the_server(tmp_path: Path) -> None:
    if os.geteuid() == 0:
        pytest.skip("run as root, the refusal for a non-root caller does not apply")
    run = run_setup(tmp_path)
    assert run.returncode != 0
    assert "run this as root" in run.stderr
    assert list(tmp_path.iterdir()) == []


def test_deploy_dry_run_changes_nothing(tmp_path: Path) -> None:
    if shutil.which("rsync") is None:
        pytest.skip("rsync is not installed here")
    target = tmp_path / "target"
    target.mkdir()
    run = subprocess.run(
        ["bash", str(ROOT / "roundtable" / "deploy.sh"), "--dry-run"],
        capture_output=True,
        text=True,
        env=dict(os.environ, NEREUS_RT_TARGET=str(target) + "/"),
        timeout=120,
    )
    assert run.returncode == 0, run.stdout + run.stderr
    assert "Dry run only." in run.stdout
    assert "nereus_roundtable/service.py" in run.stdout
    assert "static/roundtable.js" in run.stdout
    assert list(target.iterdir()) == []


def snapshot_db(path: Path, marker: str) -> None:
    db = sqlite3.connect(str(path))
    db.execute("CREATE TABLE t (v TEXT)")
    db.execute("INSERT INTO t VALUES (?)", (marker,))
    db.commit()
    db.close()


def fake_ssh(folder: Path) -> Path:
    """An ssh that runs the remote command here, ignoring the host."""
    script = folder / "fake-ssh"
    script.write_text('#!/bin/sh\nshift\nexec sh -c "$*"\n', "utf-8")
    script.chmod(script.stat().st_mode | stat.S_IXUSR)
    return script


def test_pull_backup_takes_the_newest_and_keeps_seven(tmp_path: Path) -> None:
    remote = tmp_path / "remote"
    remote.mkdir()
    snapshot_db(remote / "roundtable-2026-11-09.db", "older")
    snapshot_db(remote / "roundtable-2026-11-10.db", "newest")
    (remote / "roundtable-2026-11-10.db.part").write_bytes(b"partial")
    local = tmp_path / "local"
    local.mkdir()
    for day in range(1, 9):
        snapshot_db(local / f"roundtable-2026-11-0{day}.db", f"day {day}")
    (local / "notes.txt").write_text("kept", "utf-8")
    env = dict(
        os.environ,
        NEREUS_RT_SSH=str(fake_ssh(tmp_path)),
        NEREUS_RT_BACKUP_SOURCE="root@server.invalid",
        NEREUS_RT_BACKUP_REMOTE_DIR=str(remote),
        NEREUS_RT_BACKUP_DIR=str(local),
        NEREUS_RT_BACKUP_TODAY="2026-11-10",
    )
    script = str(DEPLOY / "pull-backup.sh")
    before = tree(local)
    dry = subprocess.run(["bash", script, "--dry-run"], capture_output=True, text=True, env=env, timeout=60)
    assert dry.returncode == 0, dry.stdout + dry.stderr
    assert "Would pull it to" in dry.stdout
    assert dry.stdout.count("Would remove:") == 2
    assert tree(local) == before
    run = subprocess.run(["bash", script], capture_output=True, text=True, env=env, timeout=60)
    assert run.returncode == 0, run.stdout + run.stderr
    names = sorted(p.name for p in local.iterdir())
    # The 8 here and the one pulled: the oldest 2 go, other files stay.
    assert names == sorted([f"roundtable-2026-11-0{d}.db" for d in range(3, 9)] + ["roundtable-2026-11-10.db", "notes.txt"])
    assert filecmp.cmp(str(local / "roundtable-2026-11-10.db"), str(remote / "roundtable-2026-11-10.db"), shallow=False)
    # A second run has it already and removes nothing more.
    again = subprocess.run(["bash", script], capture_output=True, text=True, env=env, timeout=60)
    assert again.returncode == 0, again.stdout + again.stderr
    assert "Already here" in again.stdout
    assert sorted(p.name for p in local.iterdir()) == names


def test_pull_backup_refuses_a_broken_copy(tmp_path: Path) -> None:
    remote = tmp_path / "remote"
    remote.mkdir()
    (remote / "roundtable-2026-11-10.db").write_bytes(b"not a database at all" * 100)
    local = tmp_path / "local"
    env = dict(
        os.environ,
        NEREUS_RT_SSH=str(fake_ssh(tmp_path)),
        NEREUS_RT_BACKUP_REMOTE_DIR=str(remote),
        NEREUS_RT_BACKUP_DIR=str(local),
    )
    run = subprocess.run(["bash", str(DEPLOY / "pull-backup.sh")], capture_output=True, text=True, env=env, timeout=60)
    assert run.returncode != 0
    assert "roundtable-2026-11-10.db" in run.stderr
    assert sorted(p.name for p in local.iterdir()) == []


def test_pull_backup_ages_out_old_copies_when_nothing_new_comes(tmp_path: Path) -> None:
    remote = tmp_path / "remote"
    remote.mkdir()
    # The server has skipped nights: its newest is days old and already here.
    snapshot_db(remote / "roundtable-2026-11-05.db", "newest there")
    local = tmp_path / "local"
    local.mkdir()
    for day in (2, 3, 4, 5):
        snapshot_db(local / f"roundtable-2026-11-0{day}.db", f"day {day}")
    env = dict(
        os.environ,
        NEREUS_RT_SSH=str(fake_ssh(tmp_path)),
        NEREUS_RT_BACKUP_REMOTE_DIR=str(remote),
        NEREUS_RT_BACKUP_DIR=str(local),
        NEREUS_RT_BACKUP_TODAY="2026-11-11",
    )
    script = str(DEPLOY / "pull-backup.sh")
    dry = subprocess.run(["bash", script, "--dry-run"], capture_output=True, text=True, env=env, timeout=60)
    assert dry.returncode == 0, dry.stdout + dry.stderr
    assert dry.stdout.count("Would remove:") == 2
    assert len(list(local.iterdir())) == 4
    run = subprocess.run(["bash", script], capture_output=True, text=True, env=env, timeout=60)
    assert run.returncode == 0, run.stdout + run.stderr
    assert "Already here" in run.stdout
    # 11-04 is exactly 7 days before 11-11 and stays; older ones go.
    assert sorted(p.name for p in local.iterdir()) == ["roundtable-2026-11-04.db", "roundtable-2026-11-05.db"]


def test_pull_backup_ages_out_old_copies_when_the_pull_fails(tmp_path: Path) -> None:
    local = tmp_path / "local"
    local.mkdir()
    for day in (1, 2, 8, 9):
        snapshot_db(local / f"roundtable-2026-11-0{day}.db", f"day {day}")
    (local / "notes.txt").write_text("kept", "utf-8")
    failing = tmp_path / "failing-ssh"
    failing.write_text("#!/bin/sh\nexit 255\n", "utf-8")
    failing.chmod(failing.stat().st_mode | stat.S_IXUSR)
    env = dict(
        os.environ,
        NEREUS_RT_SSH=str(failing),
        NEREUS_RT_BACKUP_DIR=str(local),
        NEREUS_RT_BACKUP_TODAY="2026-11-10",
    )
    run = subprocess.run(["bash", str(DEPLOY / "pull-backup.sh")], capture_output=True, text=True, env=env, timeout=60)
    assert run.returncode != 0
    assert "could not list" in run.stderr
    assert sorted(p.name for p in local.iterdir()) == ["notes.txt", "roundtable-2026-11-08.db", "roundtable-2026-11-09.db"]


# ---------------------------------------------------------------- website


def test_published_fixtures_match_the_repository_byte_for_byte() -> None:
    source = ROOT / "roundtable" / "conformance" / "v1"
    published = ROOT / "website" / "public" / "roundtable" / "conformance" / "v1"
    want = {p.relative_to(source).as_posix(): p.read_bytes() for p in source.rglob("*") if p.is_file()}
    got = {p.relative_to(published).as_posix(): p.read_bytes() for p in published.rglob("*") if p.is_file()}
    assert sorted(got) == sorted(want)
    for name, data in want.items():
        assert got[name] == data, name


def test_api_page_is_published() -> None:
    page = (ROOT / "website" / "public" / "roundtable" / "api.html").read_text("utf-8")
    assert "<title>Roundtable protocol version 1 | NereusSDR</title>" in page
    assert '<link rel="canonical" href="https://nereussdr.com/roundtable/api.html">' in page
