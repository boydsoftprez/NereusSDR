"""Exercise the release CLI against real Git tags with controlled dates."""

import os
from pathlib import Path
import subprocess
import sys

import pytest

SCRIPT = Path(__file__).resolve().parents[2] / "scripts" / "release-version.py"
SEPTEMBER = "2026-09-24T11:00:00-04:00"


class Repository:
    def __init__(self, path):
        self.path = path
        self.git("init", "-q")
        for key, value in {
            "user.name": "Release Test",
            "user.email": "release-test@example.invalid",
            "commit.gpgsign": "false",
            "tag.gpgSign": "false",
        }.items():
            self.git("config", key, value)
        self.commit("0.5.2")

    def git(self, *args, date=SEPTEMBER):
        env = dict(os.environ, GIT_AUTHOR_DATE=date, GIT_COMMITTER_DATE=date)
        return subprocess.run(
            ["git", *args], cwd=self.path, env=env,
            capture_output=True, text=True, check=True,
        ).stdout.strip()

    def commit(self, version, date=SEPTEMBER):
        (self.path / "CMakeLists.txt").write_text(
            f"project(NereusSDR VERSION {version} LANGUAGES CXX)\n"
        )
        self.git("add", "CMakeLists.txt")
        self.git("commit", "-qm", "Fixture", "--allow-empty", date=date)

    def tag(self, name, date=SEPTEMBER, lightweight=False):
        args = ["tag", name] if lightweight else ["tag", "-a", name, "-m", name]
        self.git(*args, date=date)

    def run(self, *args, explicit_repo=True):
        command = [sys.executable, str(SCRIPT), *args]
        if explicit_repo:
            command += ["--repo", str(self.path)]
        return subprocess.run(command, cwd=self.path, capture_output=True, text=True)


@pytest.fixture
def repo(tmp_path):
    return Repository(tmp_path)


def assert_success(result, expected):
    assert result.returncode == 0, result.stderr
    assert result.stdout == expected + "\n"
    assert result.stderr == ""


@pytest.mark.parametrize("tags,date,expected", [
    (["v0.5.2", "v0.4.1-rc1", "prerebase-x"], "2026-09-24", "2026.9.0"),
    (["v2026.9.0"], "2026-09-24", "2026.9.1"),
    (["v2026.9.0", "v2026.9.1-rc1"], "2026-09-24", "2026.9.1"),
    (["ios-v2026.9.0", "ios-v2026.9.1", "v2026.9.0-test1"], "2026-09-24", "2026.9.0"),
    (["v2026.9.0", "v2026.9.2"], "2026-09-24", "2026.9.3"),
    (["v2026.9.0", "v2026.9.1"], "2026-10-02", "2026.10.0"),
    (["v2026.12.3"], "2027-01-05", "2027.1.0"),
    (["v2026.9.9", "v2026.9.10"], "2026-09-24", "2026.9.11"),
    (["v2026.09.9", "v2026.9.01", "v2026.9.9-alpha1"], "2026-09-24", "2026.9.0"),
])
def test_next_counts_only_valid_finals_in_ship_month(repo, tags, date, expected):
    for tag in tags:
        repo.tag(tag)
    assert_success(repo.run("next", "--date", date), expected)


@pytest.mark.parametrize("tags,expected", [
    (["v2026.9.0", "v2026.9.1-rc1"], "2026.9.1-rc2"),
    (["v2026.9.0"], "2026.9.1-rc1"),
    (["v2026.9.0-rc1", "v2026.9.0-rc9", "v2026.9.0-rc10"], "2026.9.0-rc11"),
    (["v2026.9.1-rc5", "v2026.9.0-rc0", "v2026.9.0-rc01", "ios-v2026.9.0-rc7"], "2026.9.0-rc1"),
])
def test_next_candidate_counts_only_candidates_for_next_base(repo, tags, expected):
    for tag in tags:
        repo.tag(tag)
    assert_success(repo.run("next", "--rc", "--date", "2026-09-24"), expected)


@pytest.mark.parametrize("tags,expected", [
    (["v0.5.2", "v0.4.1-rc3", "ios-v2026.9.0", "prerebase-x"], "v0.5.2"),
    (["v0.5.2", "v2026.9.0", "v2026.9.1-rc1"], "v2026.9.0"),
    (["v2026.9.99", "v2026.10.0", "v2025.12.9"], "v2026.10.0"),
    (["v0.9.9", "v0.10.0", "v0.10.1-test"], "v0.10.0"),
])
def test_last_returns_numerically_highest_desktop_final(repo, tags, expected):
    for tag in tags:
        repo.tag(tag)
    assert_success(repo.run("last"), expected)


def test_last_reports_no_desktop_final(repo):
    repo.tag("ios-v2026.9.0")
    result = repo.run("last")
    assert result.returncode == 1
    assert result.stdout == ""
    assert "no final desktop release" in result.stderr.lower()
    assert len(result.stderr.splitlines()) == 1


@pytest.mark.parametrize("tag,version,date,lightweight", [
    ("v2026.9.0", "2026.9.0", SEPTEMBER, False),
    ("v2026.9.1-rc1", "2026.9.1", SEPTEMBER, False),
    ("v2026.9.0-test1", "2026.9.0", SEPTEMBER, False),
    ("v2026.9.0-issue83.fix", "2026.9.0", SEPTEMBER, False),
    ("v2026.10.0", "2026.10.0", "2026-10-01T00:30:00-04:00", False),
    ("v2026.9.0", "2026.9.0", "2026-09-24T12:00:00-04:00", True),
])
def test_check_accepts_tag_month_and_cmake_base(repo, tag, version, date, lightweight):
    repo.commit(version, date=date)
    repo.tag(tag, date=date, lightweight=lightweight)
    result = repo.run("check", tag)
    assert result.returncode == 0, result.stderr
    assert result.stdout.startswith("ok:")
    assert len(result.stdout.splitlines()) == 1
    assert result.stderr == ""


@pytest.mark.parametrize("tag", [
    "v2026.09.0", "v2026.13.0", "v2026.9.01", "v26.9.0", "v0.5.3",
    "v2026.0.0", "v2026.9.0-", "v2026.9.0-rc_1", "ios-v2026.9.0",
])
def test_check_rejects_shape_before_checking_existence(repo, tag):
    result = repo.run("check", tag)
    assert result.returncode == 1
    assert result.stdout == ""
    assert "vYYYY.M.N[-suffix]" in result.stderr
    assert "1 to 12" in result.stderr
    assert "no leading zeros" in result.stderr
    assert len(result.stderr.splitlines()) == 1


def test_check_requires_existing_tag_not_same_named_branch(repo):
    repo.git("branch", "v2026.9.5")
    result = repo.run("check", "v2026.9.5")
    assert result.returncode == 1
    assert "v2026.9.5" in result.stderr
    assert "does not exist" in result.stderr


@pytest.mark.parametrize("tag,version,date,lightweight,month", [
    ("v2026.10.0", "2026.10.0", "2026-09-30T22:00:00-04:00", False, "2026-09-30"),
    ("v2026.9.0", "2026.9.0", "2026-08-31T12:00:00-04:00", True, "2026-08-31"),
    ("v2026.9.0", "0.5.2", "2026-08-31T12:00:00-04:00", False, "2026-08-31"),
])
def test_check_uses_recorded_timezone_and_checks_date_before_cmake(repo, tag, version, date, lightweight, month):
    repo.commit(version, date=date)
    repo.tag(tag, date=date, lightweight=lightweight)
    result = repo.run("check", tag)
    assert result.returncode == 1
    assert month in result.stderr
    assert "(-04:00)" in result.stderr
    assert "was tagged" in result.stderr
    assert len(result.stderr.splitlines()) == 1


def test_check_uses_annotated_tagger_date_instead_of_commit_date(repo):
    repo.commit("2026.9.0", date="2026-08-31T12:00:00-04:00")
    repo.tag("v2026.9.0", date=SEPTEMBER)
    assert repo.run("check", "v2026.9.0").returncode == 0


def test_check_rejects_cmake_mismatch_at_tag_even_when_worktree_matches(repo):
    repo.tag("v2026.9.0")
    repo.commit("2026.9.0")
    result = repo.run("check", "v2026.9.0")
    assert result.returncode == 1
    assert "0.5.2" in result.stderr
    assert "2026.9.0" in result.stderr


def test_check_reads_matching_cmake_from_tag_not_worktree(repo):
    repo.commit("2026.9.0")
    repo.tag("v2026.9.0")
    repo.commit("0.5.2")
    assert repo.run("check", "v2026.9.0").returncode == 0


def test_repo_defaults_to_current_directory(repo):
    repo.tag("v0.5.2")
    assert_success(repo.run("last", explicit_repo=False), "v0.5.2")


def test_date_defaults_to_local_today(repo):
    from datetime import date
    today = date.today()
    assert_success(repo.run("next"), f"{today.year}.{today.month}.0")
