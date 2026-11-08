#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Modification history (NereusSDR):
#   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 20.
#                AI-assisted implementation via Anthropic Claude Code.
#   2026-11-06 - J.J. Boyd (KG4VCF), a key that leaves the deck while down lets go.
#                AI-assisted implementation via Anthropic Claude Code.
"""python3 test_link.py                       link.py against a stub of the window
python3 test_link.py --integration <home>  a client tst_deck_plugin_clients drives through stdin

main.py and the action can't import here without StreamController. main.py is only compiled, and the
action runs on stand-ins for the host's classes, called the way the host calls it. No host has run this.
"""
import importlib.util
import json
import os
import socket
import sys
import tempfile
import threading
import time
import types
import unittest
from unittest import mock

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import link  # noqa: E402

WAIT_SECONDS = 30.0


def integration(home):
    client = link.DeckClient("streamcontroller", "test", home=home)
    client.start()
    # Reads its orders only once connected: a press before that is dropped, by design.
    while not client.connected():
        time.sleep(0.01)
    for line in sys.stdin:
        parts = line.split()
        if parts[0] == "press":
            client.key_down(parts[1])
        elif parts[0] == "release":
            client.key_up(parts[1])
        elif parts[0] == "turn":
            client.dial_ticks(parts[1], int(parts[2]))
    client.stop()


def wait_for(condition, what):
    deadline = time.monotonic() + WAIT_SECONDS
    while not condition():
        if time.monotonic() > deadline:
            raise AssertionError("Timed out waiting for " + what)
        time.sleep(0.005)


class StubWindow:
    """Answers each hello, and keeps every line it hears."""

    def __init__(self, path, answer=True, hello_split=None):
        self.path = path
        self.answer = answer
        self.hello_split = hello_split
        self.lines = []
        self.clients = []
        self.server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.server.bind(path)
        self.server.listen(4)
        threading.Thread(target=self._accept, daemon=True).start()

    def _accept(self):
        while True:
            try:
                sock, _ = self.server.accept()
            except OSError:
                return
            self.clients.append(sock)
            threading.Thread(target=self._read, args=(sock,), daemon=True).start()

    def _read(self, sock):
        buffer = b""
        try:
            while True:
                chunk = sock.recv(4096)
                if not chunk:
                    return
                buffer += chunk
                while b"\n" in buffer:
                    line, buffer = buffer.split(b"\n", 1)
                    message = json.loads(line)
                    self.lines.append(message)
                    if message["type"] == "hello" and self.answer:
                        text = json.dumps({"type": "hello", "protocol": 1, "actions": [{"id": "x"}]}).encode() + b"\n"
                        if self.hello_split:
                            sock.sendall(text[:self.hello_split])
                            time.sleep(0.02)
                            text = text[self.hello_split:]
                        sock.sendall(text)
        except OSError:
            return

    def broadcast(self, data):
        for sock in self.clients:
            sock.sendall(data)

    def close(self):
        self.server.close()
        for sock in self.clients:
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            sock.close()
        self.clients = []
        os.unlink(self.path)


class LinkTest(unittest.TestCase):
    def setUp(self):
        # /tmp, not the system's temp folder: a socket's path has a short limit.
        self.home_holder = tempfile.TemporaryDirectory(dir="/tmp", prefix="ltp-")
        self.addCleanup(self.home_holder.cleanup)
        self.home = self.home_holder.name
        self.states = []
        self.displays = []
        self.ups = 0
        self.downs = 0
        self.client = link.DeckClient("streamcontroller", "9", home=self.home, retry_seconds=0.02, answer_seconds=0.2,
                                      on_connected=self._connected, on_disconnected=self._disconnected,
                                      on_state=lambda i, on: self.states.append((i, on)),
                                      on_display=self.displays.append)
        self.addCleanup(self.client.stop)

    def _connected(self, _actions):
        self.ups += 1

    def _disconnected(self):
        self.downs += 1

    def discovery(self, name, socket_name, age, protocol=1):
        folder = link.deck_folder(self.home)
        os.makedirs(folder, exist_ok=True)
        path = os.path.join(folder, name + ".json")
        with open(path, "w", encoding="utf-8") as handle:
            json.dump({"protocol": protocol, "path": os.path.join(self.home, socket_name), "pid": 1, "profile": ""}, handle)
        when = time.time() - age
        os.utime(path, (when, when))

    def window(self, name="w", age=1, **options):
        stub = StubWindow(os.path.join(self.home, name + ".sock"), **options)
        self.addCleanup(lambda: stub.clients and stub.close())
        self.discovery(name, name + ".sock", age)
        return stub

    def test_retries_every_two_seconds_by_default(self):
        self.assertEqual(link.RETRY_SECONDS, 2.0)

    def test_says_hello(self):
        stub = self.window()
        self.client.start()
        wait_for(lambda: self.ups == 1, "connected")
        self.assertEqual(stub.lines[0], {"type": "hello", "plugin": "streamcontroller", "version": "9"})

    def test_newest_first_and_skips_what_does_not_answer(self):
        silent = self.window("s", age=2, answer=False)
        good = self.window("g", age=4)
        older = self.window("o", age=5)
        self.discovery("dead", "nothing-here.sock", age=1)
        self.discovery("other", "x.sock", age=3, protocol=99)
        with open(os.path.join(link.deck_folder(self.home), "broken.json"), "w", encoding="utf-8") as handle:
            handle.write("{half")
        self.client.start()
        wait_for(lambda: self.ups == 1, "connected")
        self.assertEqual(len(silent.lines), 1)
        self.assertEqual(len(good.lines), 1)
        self.assertEqual(older.lines, [])

    def test_retries_until_a_window_comes_and_again_when_it_goes(self):
        self.client.start()     # no folder at all yet
        time.sleep(0.06)
        self.assertFalse(self.client.connected())
        first = self.window()
        wait_for(lambda: self.ups == 1, "connected")
        first.close()
        wait_for(lambda: self.downs == 1, "disconnected")
        second = StubWindow(first.path)
        self.addCleanup(lambda: second.clients and second.close())
        wait_for(lambda: self.ups == 2, "connected again")

    def test_buffers_partial_lines(self):
        stub = self.window(hello_split=5)
        self.client.start()
        wait_for(lambda: self.ups == 1, "connected")
        wait_for(lambda: stub.clients, "the stub's client")
        stub.broadcast(b'{"type":"state","id":"tx.m')
        time.sleep(0.02)
        self.assertEqual(self.states, [])
        stub.broadcast(b'ox","on":true}\n{"type":"state","id":"mute","on":false}\n{"not json\n')
        wait_for(lambda: len(self.states) == 2, "two states")
        self.assertEqual(self.states, [("tx.mox", True), ("mute", False)])

    def test_surfaces_the_display(self):
        stub = self.window()
        self.client.start()
        wait_for(lambda: self.ups == 1, "connected")
        stub.broadcast(b'{"type":"display","frequency":"14.250.000","mode":"USB","filter":"2.7K","slice":"A"}\n')
        wait_for(lambda: self.displays, "the display")
        self.assertEqual(self.displays[0], {"frequency": "14.250.000", "mode": "USB", "filter": "2.7K", "slice": "A"})

    def test_keys_and_ticks(self):
        stub = self.window()
        self.client.key_down("tx.hold")     # nothing to say it to, and it must not wait for one
        self.client.start()
        wait_for(lambda: self.ups == 1, "connected")
        self.client.key_down("tx.hold")
        self.client.key_down("mute", "B")
        self.client.key_down("mute", "")    # no slice chosen: a pin of nothing is refused, so none is sent
        self.client.key_up("tx.hold")
        self.client.dial_ticks("tune.up", 3)
        self.client.dial_ticks("tune.down", -2, "C")
        self.client.dial_ticks("tune.up", 0)
        self.client.dial_ticks("mute", 1)
        wait_for(lambda: len(stub.lines) >= 7, "seven lines")
        self.assertEqual(stub.lines[1:], [
            {"type": "press", "id": "tx.hold"},
            {"type": "press", "id": "mute", "pin": "B"},
            {"type": "press", "id": "mute"},
            {"type": "release", "id": "tx.hold"},
            {"type": "turn", "idUp": "tune.up", "idDown": "tune.down", "steps": 3},
            {"type": "turn", "idUp": "tune.up", "idDown": "tune.down", "steps": -2, "pin": "C"},
        ])

    def test_pairs_what_a_dial_turns(self):
        self.assertEqual(link.turn_pair("volume.down"), ("volume.up", "volume.down"))
        self.assertEqual(link.turn_pair("tune.up10"), ("tune.up10", "tune.down10"))
        self.assertIsNone(link.turn_pair("tx.hold"))
        self.assertIsNone(link.valid_pin("E"))
        self.assertIsNone(link.valid_pin("a"))

    def test_host_files_compile(self):
        for relative in ("main.py", os.path.join("actions", "deck_action.py")):
            with open(os.path.join(HERE, relative), encoding="utf-8") as handle:
                compile(handle.read(), relative, "exec")


class HostAction:
    """What deck_action.py takes from StreamController's ActionBase."""

    def __init__(self, plugin_base):
        self.plugin_base = plugin_base
        self.deck_controller = types.SimpleNamespace(get_alive=lambda: True)
        self.state = 0
        self.input_state = 0
        self.present = True
        self.removed_upstream = 0

    def get_is_present(self):
        return self.present

    def get_input(self):
        return types.SimpleNamespace(state=self.input_state)

    def get_settings(self):
        return {}

    def on_removed_from_cache(self):
        self.removed_upstream += 1


class Events:
    DOWN = "down"
    UP = "up"
    TURN_CW = "cw"
    TURN_CCW = "ccw"


def load_deck_action():
    host_input = types.SimpleNamespace(Key=types.SimpleNamespace(Events=Events),
                                       Dial=types.SimpleNamespace(Events=Events))
    stand_ins = {
        "src": types.ModuleType("src"),
        "src.backend": types.ModuleType("src.backend"),
        "src.backend.PluginManager": types.ModuleType("src.backend.PluginManager"),
        "src.backend.PluginManager.ActionBase": types.SimpleNamespace(ActionBase=HostAction),
        "src.backend.DeckManagement": types.ModuleType("src.backend.DeckManagement"),
        "src.backend.DeckManagement.InputIdentifier": types.SimpleNamespace(Input=host_input),
        "gi": types.SimpleNamespace(require_version=lambda *_: None),
        "gi.repository": types.SimpleNamespace(Adw=None, GLib=None, Gtk=None),
    }
    with mock.patch.dict(sys.modules, stand_ins):
        spec = importlib.util.spec_from_file_location("deck_action_for_test",
                                                      os.path.join(HERE, "actions", "deck_action.py"))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
    return module


class HeldKeyTest(unittest.TestCase):
    def setUp(self):
        self.module = load_deck_action()
        self.sent = []
        link_stub = types.SimpleNamespace(key_down=lambda i, pin: self.sent.append(("press", i)),
                                          key_up=lambda i: self.sent.append(("release", i)))
        self.plugin = types.SimpleNamespace(link=link_stub, held=set())
        action_class = type("HoldAction", (self.module.DeckAction,), {"ACTION_ID": "tx.hold"})
        self.action = action_class(self.plugin)

    def press(self):
        self.action.event_callback(Events.DOWN, None)
        self.assertEqual(self.sent, [("press", "tx.hold")])

    def released_once(self):
        self.assertEqual(self.sent, [("press", "tx.hold"), ("release", "tx.hold")])
        self.assertEqual(self.plugin.held, set())

    def test_an_ordinary_press_and_release(self):
        self.press()
        self.assertEqual(self.plugin.held, {self.action})
        self.action.event_callback(Events.UP, None)
        self.released_once()

    def test_a_release_with_no_press_says_nothing(self):
        # The key went down on the page before this one.
        self.action.event_callback(Events.UP, None)
        self.assertEqual(self.sent, [])

    def test_removed_while_down_lets_go_once(self):
        self.press()
        self.action.on_removed_from_cache()
        self.released_once()
        self.assertEqual(self.action.removed_upstream, 1)   # the host's own tidying still runs
        self.action.event_callback(Events.UP, None)
        self.released_once()

    def test_removed_while_up_says_nothing(self):
        self.action.on_removed_from_cache()
        self.assertEqual(self.sent, [])
        self.assertEqual(self.action.removed_upstream, 1)

    def test_another_page_lets_go_once(self):
        self.press()
        self.module.let_go_of_what_left(self.plugin.held)
        self.assertEqual(len(self.sent), 1)     # still here: nothing changes
        self.action.present = False
        self.module.let_go_of_what_left(self.plugin.held)
        self.released_once()
        self.module.let_go_of_what_left(self.plugin.held)
        self.released_once()

    def test_another_state_of_the_key_lets_go_once(self):
        self.press()
        self.action.input_state = 1     # the release goes to that state's actions, not this one
        self.module.let_go_of_what_left(self.plugin.held)
        self.released_once()
        self.module.let_go_of_what_left(self.plugin.held)
        self.released_once()

    def test_a_deck_that_went_away_lets_go(self):
        self.press()
        self.action.deck_controller = types.SimpleNamespace(get_alive=lambda: False)
        self.module.let_go_of_what_left(self.plugin.held)
        self.released_once()

    def test_a_deck_that_raises_lets_go(self):
        def gone():
            raise OSError("no such device")
        self.press()
        self.action.deck_controller = types.SimpleNamespace(get_alive=gone)
        self.module.let_go_of_what_left(self.plugin.held)
        self.released_once()

    def test_the_plugin_runs_the_check(self):
        with open(os.path.join(HERE, "main.py"), encoding="utf-8") as handle:
            text = handle.read()
        self.assertIn("let_go_of_what_left(self.held)", text)
        self.assertIn("target=self._watch_held", text)


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--integration":
        integration(sys.argv[2])
    else:
        unittest.main()
