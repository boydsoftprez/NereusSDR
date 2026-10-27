# no-port-check: NereusSDR-original.
"""The server's stops, run as its own process (__main__.py): with
--stop-on-stdin-eof, the end of standard input stops it cleanly with exit
code 0, the way SIGTERM does, on every OS; without the flag the end of
standard input does nothing and SIGTERM still stops it."""

from __future__ import annotations

import signal
import socket
import subprocess
import sys
import time
from pathlib import Path

import pytest

SERVER = Path(__file__).resolve().parent.parent / "server"
LISTEN_TIMEOUT_S = 20.0
STOP_TIMEOUT_S = 20.0


def _free_ports(count):
    held = []
    try:
        for _ in range(count):
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.bind(("127.0.0.1", 0))
            held.append(sock)
        return [sock.getsockname()[1] for sock in held]
    finally:
        for sock in held:
            sock.close()


def _start(tmp_path, *flags):
    ws_port, http_port = _free_ports(2)
    (tmp_path / "static").mkdir()
    (tmp_path / "backup").mkdir()
    config = tmp_path / "roundtable.conf"
    config.write_text(
        "[roundtable]\n"
        f"ws_listen = 127.0.0.1:{ws_port}\n"
        f"http_listen = 127.0.0.1:{http_port}\n"
        "public_host = 127.0.0.1\n"
        f"database = {tmp_path / 'roundtable.db'}\n"
        f"backup_dir = {tmp_path / 'backup'}\n"
        f"static_dir = {tmp_path / 'static'}\n"
        "log_level = info\n",
        encoding="utf-8",
    )
    log = open(tmp_path / "server.log", "wb")
    server = subprocess.Popen(
        [sys.executable, "-m", "nereus_roundtable", "--config", str(config), *flags],
        cwd=str(SERVER),
        stdin=subprocess.PIPE,
        stdout=subprocess.DEVNULL,
        stderr=log,
    )
    log.close()
    deadline = time.monotonic() + LISTEN_TIMEOUT_S
    for port in (ws_port, http_port):
        while True:
            if server.poll() is not None:
                pytest.fail("the server exited: " + _log(tmp_path))
            try:
                socket.create_connection(("127.0.0.1", port), timeout=0.3).close()
                break
            except OSError:
                if time.monotonic() > deadline:
                    server.kill()
                    server.wait()
                    pytest.fail(f"the server did not listen on {port}")
                time.sleep(0.05)
    return server


def _log(tmp_path):
    return (tmp_path / "server.log").read_text(encoding="utf-8", errors="replace")


def _wait(server):
    try:
        return server.wait(timeout=STOP_TIMEOUT_S)
    except subprocess.TimeoutExpired:
        server.kill()
        server.wait()
        pytest.fail("the server did not stop")


def test_the_end_of_stdin_stops_the_server_cleanly_with_the_flag(tmp_path):
    server = _start(tmp_path, "--stop-on-stdin-eof")
    server.stdin.close()
    assert _wait(server) == 0
    log = _log(tmp_path)
    assert "Traceback" not in log
    assert "standard input closed" in log
    assert log.rstrip().endswith("INFO stopping")


def test_without_the_flag_the_end_of_stdin_does_nothing(tmp_path):
    server = _start(tmp_path)
    server.stdin.close()
    try:
        server.wait(timeout=1.0)
        pytest.fail("the server stopped at the end of stdin without the flag")
    except subprocess.TimeoutExpired:
        pass
    server.kill()
    server.wait()
    assert "standard input closed" not in _log(tmp_path)


@pytest.mark.skipif(sys.platform == "win32", reason="Windows has no SIGTERM to send a console program")
@pytest.mark.parametrize("flags", [(), ("--stop-on-stdin-eof",)], ids=["plain", "with-flag"])
def test_sigterm_still_stops_the_server_cleanly(tmp_path, flags):
    server = _start(tmp_path, *flags)
    server.send_signal(signal.SIGTERM)
    assert _wait(server) == 0
    log = _log(tmp_path)
    assert "Traceback" not in log
    assert "Fatal Python error" not in log
    assert "standard input closed" not in log
    assert log.rstrip().endswith("INFO stopping")
    server.stdin.close()
