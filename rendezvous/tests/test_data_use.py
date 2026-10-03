# no-port-check: NereusSDR-original.
"""The data-use report (rendezvous/deploy/data-use.py): readings, the daily
line, reboots, the month's end and the threshold warning."""

import datetime
import importlib.util
from pathlib import Path

SCRIPT = Path(__file__).resolve().parent.parent / "deploy" / "data-use.py"
spec = importlib.util.spec_from_file_location("data_use", SCRIPT)
data_use = importlib.util.module_from_spec(spec)
spec.loader.exec_module(data_use)

GB = 1000 ** 3


def ts(text):
    return datetime.datetime.strptime(text, "%Y-%m-%d %H:%M").replace(tzinfo=datetime.timezone.utc).timestamp()


class Host:
    def __init__(self, tmp_path):
        self.tmp = tmp_path
        (tmp_path / "net" / "eth0" / "statistics").mkdir(parents=True)
        self.boot("boot-1")
        self.sent(0)

    def boot(self, boot_id):
        (self.tmp / "boot_id").write_text(boot_id + "\n")

    def sent(self, n):
        (self.tmp / "net" / "eth0" / "statistics" / "tx_bytes").write_text("%d\n" % n)

    def read(self, when, threshold=1000, capsys=None):
        argv = [
            "--interface", "eth0", "--threshold-gb", str(threshold),
            "--state", str(self.tmp / "state.json"), "--statistics", str(self.tmp / "net"),
            "--boot-id", str(self.tmp / "boot_id"), "--now", str(ts(when)),
        ]
        assert data_use.main(argv) == 0
        return capsys.readouterr().out.splitlines()


def test_first_reading_logs_once_a_day_and_counts_from_its_own_time(tmp_path, capsys):
    host = Host(tmp_path)
    host.sent(5 * GB)
    lines = host.read("2026-09-10 12:00", capsys=capsys)
    # The 5 GB sent before the report existed are not counted.
    assert lines == ["<6>data use: 0.00 GB so far this month (2026-09) on eth0, counted from 2026-09-10 12:00 UTC; "
                     "report threshold 1000 GB"]
    host.sent(7 * GB)
    assert host.read("2026-09-10 13:00", capsys=capsys) == []
    host.sent(9 * GB)
    lines = host.read("2026-09-11 00:00", capsys=capsys)
    assert lines == ["<6>data use: 4.00 GB so far this month (2026-09) on eth0, counted from 2026-09-10 12:00 UTC; "
                     "report threshold 1000 GB"]


def test_a_reboot_counts_the_counter_since_boot_and_says_the_total_is_short(tmp_path, capsys):
    host = Host(tmp_path)
    host.sent(100)
    host.read("2026-09-01 00:10", capsys=capsys)
    host.sent(100 + 2 * GB)
    host.read("2026-09-01 01:00", capsys=capsys)
    host.boot("boot-2")
    host.sent(3 * GB)
    lines = host.read("2026-09-02 00:00", capsys=capsys)
    assert len(lines) == 1
    assert lines[0].startswith("<6>data use: 5.00 GB so far this month (2026-09) on eth0, short:")


def test_the_month_ends_with_its_total_and_the_next_starts_at_zero(tmp_path, capsys):
    host = Host(tmp_path)
    host.read("2026-09-30 22:00", capsys=capsys)
    host.sent(1 * GB)
    host.read("2026-09-30 23:00", capsys=capsys)
    host.sent(3 * GB)
    lines = host.read("2026-10-01 00:00", capsys=capsys)
    assert lines == [
        "<6>data use: 2026-09 ended at 3.00 GB sent on eth0",
        "<6>data use: 0.00 GB so far this month (2026-10) on eth0; report threshold 1000 GB",
    ]


def test_past_the_threshold_a_warning_line_once_a_day(tmp_path, capsys):
    host = Host(tmp_path)
    host.read("2026-09-01 00:00", capsys=capsys)
    host.sent(3 * GB)
    lines = host.read("2026-09-02 00:00", threshold=2, capsys=capsys)
    assert lines[0].startswith("<6>data use: 3.00 GB so far")
    assert lines[1].startswith("<4>data use: warning: 3.00 GB sent this month on eth0, past the report threshold of 2 GB")
    host.sent(4 * GB)
    assert host.read("2026-09-02 01:00", threshold=2, capsys=capsys) == []


def test_a_missing_interface_is_an_error_line(tmp_path, capsys):
    argv = ["--interface", "nope", "--threshold-gb", "1", "--state", str(tmp_path / "s"),
            "--statistics", str(tmp_path), "--boot-id", str(tmp_path / "b")]
    assert data_use.main(argv) == 1
    assert capsys.readouterr().out.startswith("<3>data use: cannot read the counter for nope")
