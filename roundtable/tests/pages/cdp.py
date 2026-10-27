# no-port-check: NereusSDR-original.
"""Headless Chrome driven over the DevTools protocol on a pipe, for the
web page checks. No WebSocket library: Chrome is started with
--remote-debugging-pipe, reads commands on fd 3 and writes answers on fd 4,
each a JSON text ended by a NUL byte.

Every run gets a fresh --user-data-dir under the directory it is given, so
no profile of the person using the machine is ever opened.
"""

from __future__ import annotations

import base64
import json
import os
import select
import shutil
import subprocess
import tempfile
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence

CHROME = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"


def chrome_path() -> Optional[str]:
    path = os.environ.get("ROUNDTABLE_CHROME", CHROME)
    return path if os.path.exists(path) else None


class CdpError(RuntimeError):
    pass


class Chrome:
    """One headless Chrome with one page."""

    def __init__(self, scratch: Path, width: int = 1480, height: int = 1000, args: Sequence[str] = ()) -> None:
        path = chrome_path()
        if path is None:
            raise CdpError("Chrome is not installed")
        scratch.mkdir(parents=True, exist_ok=True)
        self.profile = tempfile.mkdtemp(prefix="chrome-", dir=str(scratch))
        to_chrome_r, self._to_chrome = os.pipe()
        self._from_chrome, from_chrome_w = os.pipe()

        def child_fds() -> None:
            os.dup2(to_chrome_r, 3)
            os.dup2(from_chrome_w, 4)

        command = [
            path,
            "--headless=new",
            "--disable-gpu",
            "--hide-scrollbars",
            f"--window-size={width},{height}",
            f"--user-data-dir={self.profile}",
            "--remote-debugging-pipe",
            "--no-first-run",
            "--no-default-browser-check",
            "--disable-extensions",
            "--disable-background-networking",
            "--disable-sync",
            "--disable-component-update",
            "--password-store=basic",
            "--use-mock-keychain",
            "--mute-audio",
            *args,
            "about:blank",
        ]
        self.proc = subprocess.Popen(
            command,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            preexec_fn=child_fds,
            pass_fds=(3, 4),
            close_fds=True,
        )
        os.close(to_chrome_r)
        os.close(from_chrome_w)
        self._buf = b""
        self._id = 0
        self._aux = 1000000000
        self.events: List[Dict[str, Any]] = []
        self.session = ""
        self.width = width
        self.height = height
        target = self.call("Target.createTarget", {"url": "about:blank"}, browser=True)["targetId"]
        self.session = self.call("Target.attachToTarget", {"targetId": target, "flatten": True}, browser=True)["sessionId"]
        for domain in ("Page", "Runtime", "Log"):
            self.call(f"{domain}.enable")
        self.size(width, height)

    # ----------------------------------------------------------- transport

    def _send(self, msg: Dict[str, Any]) -> None:
        data = json.dumps(msg).encode("utf-8") + b"\0"
        while data:
            n = os.write(self._to_chrome, data)
            data = data[n:]

    def _read_one(self, deadline: float) -> Dict[str, Any]:
        while b"\0" not in self._buf:
            left = deadline - time.monotonic()
            if left <= 0:
                raise CdpError("Chrome did not answer in time")
            ready, _, _ = select.select([self._from_chrome], [], [], left)
            if not ready:
                continue
            chunk = os.read(self._from_chrome, 1 << 20)
            if not chunk:
                raise CdpError("Chrome closed its pipe")
            self._buf += chunk
        text, _, self._buf = self._buf.partition(b"\0")
        return json.loads(text.decode("utf-8"))

    def call(self, method: str, params: Optional[Dict[str, Any]] = None, browser: bool = False, timeout: float = 30.0) -> Dict[str, Any]:
        self._id += 1
        msg: Dict[str, Any] = {"id": self._id, "method": method, "params": params or {}}
        if not browser and self.session:
            msg["sessionId"] = self.session
        self._send(msg)
        deadline = time.monotonic() + timeout
        while True:
            got = self._read_one(deadline)
            if got.get("id") == self._id:
                if "error" in got:
                    raise CdpError(f"{method}: {got['error']}")
                return got.get("result", {})
            if "method" in got:
                self.events.append(got)
                if got["method"] == "Page.javascriptDialogOpening":
                    # Never left open: a dialog would stop the page.
                    self._aux += 1
                    self._send({"id": self._aux, "method": "Page.handleJavaScriptDialog", "params": {"accept": False}, "sessionId": self.session})

    def pump(self, seconds: float) -> None:
        """Collect events for a while (the page keeps running)."""
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            ready, _, _ = select.select([self._from_chrome], [], [], max(0.0, end - time.monotonic()))
            if ready:
                chunk = os.read(self._from_chrome, 1 << 20)
                if not chunk:
                    raise CdpError("Chrome closed its pipe")
                self._buf += chunk
                while b"\0" in self._buf:
                    text, _, self._buf = self._buf.partition(b"\0")
                    got = json.loads(text.decode("utf-8"))
                    if "method" in got:
                        self.events.append(got)

    # ----------------------------------------------------------- page

    def size(self, width: int, height: int, mobile: bool = False) -> None:
        self.width = width
        self.height = height
        self.call(
            "Emulation.setDeviceMetricsOverride",
            {"width": width, "height": height, "deviceScaleFactor": 1, "mobile": mobile},
        )

    def goto(self, url: str, settle: float = 0.5) -> None:
        self.call("Page.navigate", {"url": url})
        self.wait("document.readyState === 'complete'", 20)
        if settle:
            self.pump(settle)

    def eval(self, expression: str, timeout: float = 30.0) -> Any:
        got = self.call(
            "Runtime.evaluate",
            {"expression": expression, "awaitPromise": True, "returnByValue": True, "userGesture": True},
            timeout=timeout,
        )
        if "exceptionDetails" in got:
            detail = got["exceptionDetails"]
            text = detail.get("exception", {}).get("description") or detail.get("text")
            raise CdpError(f"page threw: {text}")
        return got.get("result", {}).get("value")

    def wait(self, expression: str, timeout: float = 15.0, step: float = 0.1) -> Any:
        """Poll an expression until it is truthy; returns its value."""
        end = time.monotonic() + timeout
        last: Any = None
        while time.monotonic() < end:
            try:
                last = self.eval(expression)
            except CdpError:
                last = None
            if last:
                return last
            self.pump(step)
        raise CdpError(f"timed out waiting for: {expression}")

    def key(self, key: str, code: Optional[str] = None, text: Optional[str] = None) -> None:
        codes = {"ArrowDown": 40, "ArrowUp": 38, "Enter": 13, "Escape": 27, "/": 191, "Tab": 9}
        base = {"key": key, "code": code or key, "windowsVirtualKeyCode": codes.get(key, 0)}
        down = dict(base, type="keyDown")
        if text:
            down["text"] = text
        self.call("Input.dispatchKeyEvent", down)
        self.call("Input.dispatchKeyEvent", dict(base, type="keyUp"))

    def type_text(self, text: str) -> None:
        self.call("Input.insertText", {"text": text})

    def screenshot(self, path: Path) -> None:
        got = self.call("Page.captureScreenshot", {"format": "png", "captureBeyondViewport": False})
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(base64.b64decode(got["data"]))

    def exceptions(self) -> List[str]:
        out = []
        for e in self.events:
            if e.get("method") == "Runtime.exceptionThrown":
                d = e["params"]["exceptionDetails"]
                out.append(d.get("exception", {}).get("description") or d.get("text", ""))
            elif e.get("method") == "Log.entryAdded" and e["params"]["entry"].get("level") == "error":
                entry = e["params"]["entry"]
                out.append(f"{entry.get('source')}: {entry.get('text')}")
        return out

    def dialogs(self) -> int:
        return sum(1 for e in self.events if e.get("method") == "Page.javascriptDialogOpening")

    def close(self) -> None:
        try:
            self._send({"id": 0, "method": "Browser.close", "params": {}})
            self.proc.wait(timeout=10)
        except (OSError, subprocess.TimeoutExpired):
            self.proc.kill()
            self.proc.wait()
        for fd in (self._to_chrome, self._from_chrome):
            try:
                os.close(fd)
            except OSError:
                pass
        shutil.rmtree(self.profile, ignore_errors=True)
