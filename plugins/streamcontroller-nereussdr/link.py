# SPDX-License-Identifier: GPL-3.0-or-later
# Modification history (NereusSDR):
#   2026-11-06 - J.J. Boyd (KG4VCF), hot keys plan Task 20.
#                AI-assisted implementation via Anthropic Claude Code.
"""The plugin's end of the deck link to the NereusSDR window: a local socket carrying one
JSON object per line, on a thread of its own. Linux only, as StreamController is.

The same behavior as the Node plugins' link.js. A press made while no window answers is
dropped, never queued: a hold must not start later on its own. Callbacks run on the link's
thread.
"""
import json
import logging
import os
import re
import socket
import threading

log = logging.getLogger(__name__)

PROTOCOL = 1
RETRY_SECONDS = 2.0
ANSWER_SECONDS = 2.0    # how long a socket may take to answer a hello
MAX_LINE_BYTES = 65536  # as DeckLink::kMaxLineBytes


def deck_folder(home):
    return os.path.join(home, ".nereussdr", "deck")


def discover(home):
    """Every socket a readable discovery file names, newest file first."""
    folder = deck_folder(home)
    try:
        names = os.listdir(folder)
    except OSError:
        return []
    found = []
    for name in names:
        if not name.endswith(".json"):
            continue
        path = os.path.join(folder, name)
        try:
            with open(path, encoding="utf-8") as handle:
                entry = json.load(handle)
            if entry.get("protocol") == PROTOCOL and isinstance(entry.get("path"), str):
                found.append((os.path.getmtime(path), entry["path"]))
        except (OSError, ValueError, AttributeError):
            continue    # left half written, or gone since the listing
    found.sort(key=lambda item: item[0], reverse=True)
    return [path for _, path in found]


def turn_pair(action_id):
    """tune.up and tune.down, tune.up10 and tune.down10, volume.up and volume.down. A dial
    turns a pair, whichever of the two the action on it names. None for anything else."""
    match = re.fullmatch(r"(.*\.)(up|down)(\d*)", action_id)
    if match is None:
        return None
    return match.group(1) + "up" + match.group(3), match.group(1) + "down" + match.group(3)


def valid_pin(pin):
    """A slice the link accepts, or None: with no slice chosen, pin must be left out."""
    return pin if isinstance(pin, str) and re.fullmatch(r"[A-D]", pin) else None


class DeckClient:
    def __init__(self, plugin, version, home=None, retry_seconds=RETRY_SECONDS, answer_seconds=ANSWER_SECONDS,
                 on_connected=None, on_disconnected=None, on_state=None, on_display=None):
        self._plugin = plugin
        self._version = version
        self._home = home or os.path.expanduser("~")
        self._retry_seconds = retry_seconds
        self._answer_seconds = answer_seconds
        self._on_connected = on_connected
        self._on_disconnected = on_disconnected
        self._on_state = on_state
        self._on_display = on_display
        self._lock = threading.Lock()
        self._socket = None     # set only once a window has answered
        self._attempt = None
        self._stop = threading.Event()
        self._thread = None

    def start(self):
        if self._thread is None:
            self._stop.clear()
            self._thread = threading.Thread(target=self._run, name="NereusSDR deck link", daemon=True)
            self._thread.start()

    def stop(self):
        self._stop.set()
        with self._lock:
            for held in (self._socket, self._attempt):
                if held is not None:
                    try:
                        held.shutdown(socket.SHUT_RDWR)
                    except OSError:
                        pass
        if self._thread is not None:
            self._thread.join()
            self._thread = None

    def connected(self):
        with self._lock:
            return self._socket is not None

    def key_down(self, action_id, pin=None):
        self._send(self._with_pin({"type": "press", "id": action_id}, pin))

    def key_up(self, action_id):
        self._send({"type": "release", "id": action_id})

    def dial_ticks(self, action_id, ticks, pin=None):
        pair = turn_pair(action_id)
        steps = int(ticks)
        if pair is not None and steps != 0:
            self._send(self._with_pin({"type": "turn", "idUp": pair[0], "idDown": pair[1], "steps": steps}, pin))

    @staticmethod
    def _with_pin(message, pin):
        pin = valid_pin(pin)
        if pin is not None:
            message["pin"] = pin
        return message

    def _send(self, message):
        data = json.dumps(message, separators=(",", ":")).encode("utf-8") + b"\n"
        with self._lock:
            if self._socket is None:
                return
            try:
                self._socket.sendall(data)
            except OSError:
                # Ending it lets the window release what this plugin holds, and the reader
                # sees it go and retries.
                try:
                    self._socket.shutdown(socket.SHUT_RDWR)
                except OSError:
                    pass

    def _call(self, callback, *args):
        if callback is not None:
            try:
                callback(*args)
            except Exception:   # a host's error must not stop the link
                log.exception("NereusSDR deck link callback failed")

    def _run(self):
        while not self._stop.is_set():
            for path in discover(self._home):
                if self._stop.is_set():
                    break
                answered = self._try(path)
                if answered is not None:
                    self._serve(*answered)
                    break
            self._stop.wait(self._retry_seconds)

    def _try(self, path):
        """The socket and what it has said past its answer, if it answers. Else None."""
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        with self._lock:
            self._attempt = sock
        try:
            sock.settimeout(self._answer_seconds)
            sock.connect(path)
            hello = {"type": "hello", "plugin": self._plugin, "version": self._version}
            sock.sendall(json.dumps(hello).encode("utf-8") + b"\n")
            buffer = b""
            while True:
                line, buffer = self._next_line(buffer)
                if line is None:
                    chunk = sock.recv(4096)
                    if not chunk or len(buffer) > MAX_LINE_BYTES:
                        raise OSError("no answer")
                    buffer += chunk
                    continue
                message = self._parse(line)
                if message is None:
                    continue
                if message.get("type") != "hello" or message.get("protocol") != PROTOCOL:
                    raise OSError("not a window this plugin knows")
                # The timeout stays: a send to a window that has stopped reading ends the
                # connection instead of stalling the host, and _serve reads through it.
                with self._lock:
                    self._attempt = None
                    self._socket = sock
                self._call(self._on_connected, message.get("actions", []))
                return sock, buffer
        except OSError:
            with self._lock:
                self._attempt = None
            sock.close()
            return None

    def _serve(self, sock, buffer):
        try:
            while True:
                line, buffer = self._next_line(buffer)
                if line is not None:
                    message = self._parse(line)
                    if message is not None:
                        self._handle(message)
                    continue
                if len(buffer) > MAX_LINE_BYTES:
                    return
                try:
                    chunk = sock.recv(4096)
                except socket.timeout:
                    continue    # only quiet
                if not chunk:
                    return
                buffer += chunk
        except OSError:
            return
        finally:
            with self._lock:
                self._socket = None
            sock.close()
            self._call(self._on_disconnected)

    @staticmethod
    def _next_line(buffer):
        end = buffer.find(b"\n")
        if end < 0:
            return None, buffer
        return buffer[:end], buffer[end + 1:]

    @staticmethod
    def _parse(line):
        try:
            message = json.loads(line.decode("utf-8"))
        except ValueError:
            return None
        return message if isinstance(message, dict) else None

    def _handle(self, message):
        kind = message.get("type")
        if kind == "state":
            self._call(self._on_state, message.get("id"), message.get("on") is True)
        elif kind == "display":
            self._call(self._on_display, {key: message.get(key) for key in ("frequency", "mode", "filter", "slice")})
